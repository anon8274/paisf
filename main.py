# Import python modules
import sys

# Import custom modules
import helpers as hlp
import config as cfg
import run_experiments as re

# Import experiment sweeps
import experiment_vary_topo_noc
import experiment_vary_topo_pici
import experiment_vary_topo_aici
import experiment_vary_topo_wsi
import experiment_vary_in_order_ratio
import experiment_vary_msg_size
import experiment_vary_link_faults
import experiment_vary_router_faults
import experiment_vary_num_vcs
import experiment_vary_nw_size
import experiment_vary_entropy_bits
import experiment_cpu_trace
import experiment_gpu_trace


# Compose list of all experiment sweeps
experiment_sweeps = {
    "vary_topo_noc": experiment_vary_topo_noc.experiments,
    "vary_topo_pici": experiment_vary_topo_pici.experiments,
    "vary_topo_aici": experiment_vary_topo_aici.experiments,
    "vary_topo_wsi": experiment_vary_topo_wsi.experiments,
    "cpu_trace": experiment_cpu_trace.experiments,
    "gpu_trace": experiment_gpu_trace.experiments,
    "vary_in_order_ratio": experiment_vary_in_order_ratio.experiments,
    "vary_entropy_bits": experiment_vary_entropy_bits.experiments,
    "vary_msg_size": experiment_vary_msg_size.experiments,
    "vary_nw_size": experiment_vary_nw_size.experiments,
    "vary_num_vcs": experiment_vary_num_vcs.experiments,
    "vary_router_faults": experiment_vary_router_faults.experiments,
    "vary_link_faults": experiment_vary_link_faults.experiments,
}

# We run all experiments of all sweeps together since if we run sweep after sweep, the stragglers (all WSI designs which have many more routers)
# Will keep a small number of threads busy for a long time while all others are already done and wait for the next sweep to start.
def run_all_experiment_sweeps(multithreading : bool = False, verbose : bool = False):
    # Info to user
    hlp.print("The following experiment sweeps will be executed:")
    n_exp_total = 0
    all_experiments = []
    for exp_name in experiment_sweeps.keys():
        hlp.print(f"- {exp_name} containing {len(experiment_sweeps[exp_name])} experiments")
        n_exp_total += len(experiment_sweeps[exp_name])
        all_experiments.extend(experiment_sweeps[exp_name])
    hlp.print(f"Total number of experiments to run: {n_exp_total}\n")
    re.run_experiments(all_experiments, multithreading=multithreading, verbose=verbose)

if __name__ == "__main__":
    multithreading = "--multithreading" in sys.argv or "-m" in sys.argv
    verbose = "--verbose" in sys.argv or "-v" in sys.argv
    # check if "--experiment" or "-e" is provided to run only a specific experiment sweep
    if "--experiment" in sys.argv or "-e" in sys.argv:
        if "--experiment" in sys.argv:
            exp_index = sys.argv.index("--experiment") + 1
        else:
            exp_index = sys.argv.index("-e") + 1
        if exp_index < len(sys.argv):
            exp_name = sys.argv[exp_index]
            if exp_name in experiment_sweeps:
                hlp.print_magenta(f"Running only specified experiment sweep \"{exp_name}\"...")
                hlp.print("Number of experiments to run: " + str(len(experiment_sweeps[exp_name])))
                re.run_experiments(experiment_sweeps[exp_name], multithreading=multithreading, verbose=verbose)
            else:
                hlp.print_red(f"Experiment sweep \"{exp_name}\" not found. Available sweeps are: {list(experiment_sweeps.keys())}")
        else:
            hlp.print_red("No experiment sweep name provided after --experiment or -e flag.")
    else:
        hlp.print_magenta("Running all experiment sweeps...")
        run_all_experiment_sweeps(multithreading=multithreading, verbose=verbose)
