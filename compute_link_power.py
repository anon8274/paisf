# Python modules
import os
import sys

# Custom modules
import helpers as hlp
import config as cfg


def compute_link_power(adj_list : list[list[int]], lengths: list[list[float]], run_identifier : str, load : float, freq : int, cycles : int, packet_size : int, data_width : int, digits : int) -> float:
    # Find the correct link utilization stats file based on the run_identifier and load by listing all files the stats directory
    stats_dir = "./booksim2/src/stats/"
    files = os.listdir(stats_dir)
    target_file = ""
    match_count = 0
    for file in files:
        if run_identifier in file and f"{load:.{digits}f}" in file:
            target_file = file
            match_count += 1
    # Check that exactly one matching file was found
    if match_count == 0:
        hlp.print_error(f"No matching stats file found for run_identifier '{run_identifier}' and load '{load}'")
        sys.exit(1)
    elif match_count > 1:
        hlp.print_error(f"Multiple matching stats files found for run_identifier '{run_identifier}' and load '{load}'")
        sys.exit(1)
    # Load the link utilization stats from the target file and parse into a dictionary
    stats = hlp.read_json(os.path.join(stats_dir, target_file))
    link_utilization = {(x["src_router_id"], x["dst_router_id"]): x["packet_count"] for x in stats["links"]}
    # Precomputed factors 
    bits_per_packet = packet_size * data_width                                          # bits
    energy_per_packet_per_mm = bits_per_packet * cfg.energy_per_bit_per_mm_7nm_in_pj    # pJ/mm
    # Compute total energy of all links in the network
    total_energy = 0.0
    for src_rid in range(len(adj_list)):
        for (i, dst_rid) in enumerate(adj_list[src_rid]):
            # Check if this link was used
            if (src_rid, dst_rid) in link_utilization:
                packet_count = link_utilization[(src_rid, dst_rid)]
                link_length = lengths[src_rid][i]
                total_energy += packet_count * energy_per_packet_per_mm * link_length
    # Compute energy per second (power in Watts)
    total_time_seconds = cycles / freq
    power_watts = total_energy / total_time_seconds * 1e-12                             # Convert pJ to W
    # Return the result as a list with a single dictionary
    return power_watts

