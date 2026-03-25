# Python modules
import subprocess
import threading
import math
import re
import os

# Custom modules
import config as cfg

# Global lock to serialize Orion execution
# Needs to be serialized because we rewrite the config file, recompile the code and run it for each design
# NOTE: For ORION 3.0, orion_router outputs power in mW, area in um^2
_orion_lock = threading.Lock()

def export_router_to_orion(radix : int, freq : int, data_width : int, n_vcs : int, vc_buf_size : int) -> None:
    orion_config = {
        "radix" : radix,
        "frequency" : freq,
        "datawidth" : data_width,
        "virtualchannels" : n_vcs,
        "buffersize" : vc_buf_size,
    }
    # Paths
    orion_config_path = os.path.join("./orion3", "SIM_port.template")
    filled_in_config_path = os.path.join("./orion3", "SIM_port.h")
    # Read the template config file
    with open(orion_config_path, 'r') as f:
        orion_config_file = f.read()  # <-- single string
    # Fill in the template config file
    for key, value in orion_config.items():
        orion_config_file = orion_config_file.replace(f"<{key}>", str(value))
    # Write the filled-in config file
    with open(filled_in_config_path, 'w') as f:
        f.write(orion_config_file)



# Runs Orion 3.0 simulations to compute overall area and power of the network.
def run_orion(o_config : dict, adj_list : list[list[int]]) -> dict:
    with _orion_lock:
        # Identify number of routers and their radices
        radix_to_count = {}
        for rid in range(len(adj_list)):
            radix = len(adj_list[rid])
            if radix not in radix_to_count:
                radix_to_count[radix] = 0
            radix_to_count[radix] += 1
        # Compute total area and power using Orion
        orion_results = {}
        for (radix, count) in radix_to_count.items():
            # Export config to Orion
            export_router_to_orion(radix, o_config["frequency"], o_config["data_width"], o_config["n_vcs"], o_config["vc_buf_size"])
            # Compile Orion
            subprocess.run(["make", "-B"], cwd="./orion3", stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            # Run Orion
            output = subprocess.run(["./orion_router"], cwd="./orion3", capture_output=True, text=True)
            # Collect results for a given router
            router_results = {}
            for line in output.stdout.splitlines():
                if ":" in line:
                    match = re.match(r"^\s*([A-Za-z0-9_]+):\s*([0-9.eE+-]+)\s*$", line)
                    if match:
                        key, value = match.groups()
                        router_results[key] = float(value) * count
            # Add the results to the overall results, multiplying by the number of routers of this type
            for key, value in router_results.items():
                # We compute the total ourselves because we use different scaling factors for different parts
                # We are not interested in event/instruction counts
                if key.endswith("total") or key.startswith("INSTS"):
                    continue
                if key not in orion_results:
                    orion_results[key] = 0.0
                # Area results
                if key.startswith("A"):
                    # Input Buffers are SRAM and use a different scaling factor
                    if key == "Ainbuffer":
                        orion_results[key] += value * cfg.area_scaling_factor_45nm_to_7nm_sram * count
                    else:
                        orion_results[key] += value * cfg.area_scaling_factor_45nm_to_7nm * count
                # Power results
                elif key.startswith("P"):
                    orion_results[key] += value * cfg.power_scaling_factor_45nm_to_7nm * count
            # Now compute total area and power for this router
            Atotal = 0.0
            Ptotal = 0.0
            for key, value in orion_results.items():
                if key.startswith("A") and not key.endswith("total"):
                    Atotal += value
                elif key.startswith("P") and not key.endswith("total"):
                    Ptotal += value
            orion_results["Atotal"] = Atotal
            orion_results["Ptotal"] = Ptotal
        # Compute total network power (in W) and area (in mm^2)
        summary = {
                "total_router_area" : orion_results.get("Atotal", 0.0) * 1e-6,
                "total_router_power" : orion_results.get("Ptotal", 0.0) * 1e-3,
                }
        results = {"summary" : summary, "details" : orion_results, "config" : o_config}
    return results

