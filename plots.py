# Import python modules
import copy
import sys
import os
import math
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.colors import ListedColormap, BoundaryNorm
from types import SimpleNamespace
from matplotlib import cm
import numpy as np


# Import custom modules
import config as cfg
import helpers as hlp
import generate_network as gn
from main import experiment_sweeps
import fault_insertion as fi

def plot_improvement_bar_chart(experiments : list[dict], name : str, metric : str, vs : str = "us_vs_baseline") -> None:
    (data_matrix, row_combos, col_combos) = create_heatmap_plot(experiments, name, metric, do_save = False)
    # Skip if no data
    if data_matrix is None:
        return
    # Setting based on metric
    if metric in ["saturation_throughput", "average_throughput"]:
        best_function = np.nanmax
        better_comp = lambda x, y : x >= y
    elif metric in ["latency_at_0.1","zero_load_latency","energy_per_packet","total_power", "average_latency"]:
        best_function = lambda l : np.nanmin([x for x in l if not math.isnan(x) and x >= 0.0])
        better_comp = lambda x, y : x <= y
    else:
        hlp.print_red("Error: Unknown metric \"%s\" for improvement bar chart." % metric)
        return
    # Compute best our and best baseline per column
    relative_values = []
    baselines = []
    ours = []
    for col_idx, col_combo in enumerate(col_combos):
        best_baseline_val = float("nan")
        best_baseline_nam = ""
        best_ours_val = float("nan")
        best_ours_nam = ""
        for row_idx, row_combo in enumerate(row_combos):
            if vs == "us_vs_baseline":
                ours_condition = "hash" in dict(row_combo)["mod_selection_function"]
                baseline_condition = not ours_condition
            elif vs == "paisf_vs_isf":
                ours_condition = dict(row_combo)["mod_selection_function"] == "adaptive_hashed"
                baseline_condition = dict(row_combo)["mod_selection_function"] == "hashed"
            else:
                hlp.print_red("Error: Unknown vs condition \"%s\" for improvement bar chart." % vs)
                return
            if ours_condition:
                if not math.isnan(data_matrix[row_idx, col_idx]) and data_matrix[row_idx, col_idx] >= 0 and (math.isnan(best_ours_val) or best_function([best_ours_val, data_matrix[row_idx, col_idx]]) == data_matrix[row_idx, col_idx]):
                    best_ours_val = data_matrix[row_idx, col_idx]
                    best_ours_nam = " | ".join(cfg.acronyms[opt][x] for opt, x in sorted(dict(row_combo).items(), key=lambda item: experiments[0]["plot_rows"].index(item[0])) if cfg.acronyms[opt][x] is not None)
            if baseline_condition:
                if not math.isnan(data_matrix[row_idx, col_idx]) and data_matrix[row_idx, col_idx] >= 0 and (math.isnan(best_baseline_val) or best_function([best_baseline_val, data_matrix[row_idx, col_idx]]) == data_matrix[row_idx, col_idx]):
                    best_baseline_val = data_matrix[row_idx, col_idx]
                    best_baseline_nam = " | ".join(cfg.acronyms[opt][x] for opt, x in sorted(dict(row_combo).items(), key=lambda item: experiments[0]["plot_rows"].index(item[0])) if cfg.acronyms[opt][x] is not None)
        relative_value = best_ours_val * 100 / best_baseline_val if not math.isnan(best_ours_val) and not math.isnan(best_baseline_val) and best_baseline_val != 0 else float("nan")
        relative_values.append(relative_value)
        baselines.append(best_baseline_nam.replace(" ",""))
        ours.append(best_ours_nam.replace(" ",""))
    # Create bar chart
    top = 0.875 if vs == "us_vs_baseline" else 0.8
    width = 7 if vs == "us_vs_baseline" else 3.5
    left = 0.125 if vs == "us_vs_baseline" else 0.24
    bottom = 0.275 if vs == "us_vs_baseline" else 0.36
    fig, ax = plt.subplots(1,1, figsize=(width,2))
    fig.subplots_adjust(top = top, bottom = bottom, left = left, right = 0.99)
    # Plot all bars, positive values in green, negative in red
    for idx, rel_val in enumerate(relative_values):
        color = "#99CC99" if better_comp(rel_val, 100) else "#CC9999"
        ax.bar(idx, rel_val, color=color, zorder=3)
    # Plot horizontal line at 100%
    ax.axhline(100, color="#000000", linestyle="-", linewidth=1.0, zorder=2)
    # Identify lower ylim for text palcemet
    y_min= ax.get_ylim()[0]
    y_max = ax.get_ylim()[1]
    y_pos = ax.get_ylim()[0] + (ax.get_ylim()[1] - ax.get_ylim()[0]) * 0.05
    lab1, lab2 = ("Baseline", "Ours") if vs == "us_vs_baseline" else ("ISF", "PAISF")
    # Only add the annotations for ours vs. baseline to save space in the PAISF vs. ISF plot.
    if vs == "us_vs_baseline":
        for idx in range(len(relative_values)):
            # Annotate each bar with best baseline and our best as two vertical lines of text
            bl_lab = "LASH-TOR" if baselines[idx] == "DET|TOR" else baselines[idx]
            ax.text(idx - 0.125, y_pos, f"{lab1}: {bl_lab}", ha="center", va="bottom", fontsize=6, rotation=90, zorder=4)
            ax.text(idx + 0.125, y_pos, f"{lab2}: {ours[idx]}", ha="center", va="bottom", fontsize=6, rotation=90, zorder=4)
    # Print average improvement in middle of plot
    avg_rel_val = hlp.avg_geometric([x for x in relative_values if not math.isnan(x)]) # Ratio -> Geometric mean
    met_name = metric.replace("_"," ").title().replace("Average ","") # Avoid "Average Relative Average {Metric}"
    if vs == "us_vs_baseline":
        ax.set_title("Average Relative " + met_name + " of (PA)ISF w.r.t. Best Baseline: %.2f%%" % avg_rel_val, fontsize=11)
    elif vs == "paisf_vs_isf":
        ax.set_title("Average Relative " + met_name + "\nof PAISF w.r.t. ISF: %.2f%%" % avg_rel_val, fontsize=8)
    # Set ticks and labels
    if max(relative_values) < float("inf"):
        ax.set_ylim(min(0, min(relative_values) * 1.1), max(relative_values)*1.1)
    ax.set_xticks(range(len(col_combos)))
    rot = 20 if vs == "us_vs_baseline" else 70
    ax.set_xticklabels([" | ".join(f"{cfg.acronyms[opt][dict(col_combo)[opt]]}" for opt in experiments[0]["plot_cols"] if cfg.acronyms[opt][dict(col_combo)[opt]] is not None) for col_combo in col_combos], rotation=rot, ha="right", fontsize=8)
    lab2 = "(PA)ISF" if lab2 == "Ours" else lab2
    ylab = f"Relative {metric.replace('_',' ').title()} of best {lab2} w.r.t. best {lab1} [%]"
    ax.set_ylabel(hlp.split_string(ylab, 3), labelpad=1)
    # xlabel move upwards
    ax.set_xlabel(" | ".join([x.capitalize() for x in experiments[0]["plot_cols"]]), labelpad=-1)
    ax.set_ylim(y_min, y_max)
    ax.grid(axis='y', color = "#CCCCCC", linewidth=0.5, zorder=0)
    # Save plot
    plt.savefig("plots/improvement_%s_%s.%s" % (vs, name, cfg.plot_format), dpi=cfg.plot_dpi)
    plt.close()


def create_line_plots(experiments : list[dict], name : str, metric : str, do_save = True) -> None:
    color_ours = cfg.color_ours
    color_multi = cfg.color_multi
    color_det = cfg.color_det
    color_lash = cfg.color_lash
    # Settings based on metric
    if metric in ["saturation_throughput", "average_throughput"]:
        best_function = lambda l : np.nanmax(l) if len(l) > 0 and not all(math.isnan(x) for x in l) else float("nan")
    elif metric in ["latency_at_0.1","zero_load_latency", "energy_per_packet","total_power", "average_latency"]:
        best_function = lambda l : np.nanmin(l) if len(l) > 0 and not all(math.isnan(x) for x in l) else float("nan")
    else:
        hlp.print_red("Error: Unknown metric \"%s\" for heatmap plot." % metric)
        return
    # Load all results of this experiment sweep
    all_results = hlp.read_results(experiments)
    e0 = experiments[0]
    # Skip this plot if no results are available
    if len(all_results) == 0:
        hlp.print_yellow("Warning: No results found for line plot \"%s\". Skipping..." % name)
        return
    hlp.print_cyan("Creating line plot for \"%s\"" % name)
    # Identify subplots
    subplot_values = sorted(list(set([x[x["plot_subplot"]] for x in experiments])), key = lambda x : list(cfg.acronyms[e0["plot_subplot"]].keys()).index(x))
    # Create plot
    width = 12 if len(subplot_values) > 2 else 6
    fig, ax = plt.subplots(1,len(subplot_values), figsize=(width,2))
    fig.subplots_adjust(top = 0.85, bottom = 0.225, left = 0.06, right = 0.99, wspace=0.25)
    if len(subplot_values) == 1:
        ax = [ax]
    # Iterate through all subplots
    xvalues = []
    for i, subplot_value in enumerate(subplot_values):
        labels = []
        line_data = []
        line_auc = []
        line_color = []
        line_bold = []
        line_label = []
        type_label = []
        # Construct all lines
        line_combos = set()
        for exp in experiments:
            line_combo = {opt : exp[opt] for opt in e0["plot_lines"]}
            line_combos.add(frozenset(line_combo.items()))
        # Iterate through all lines
        for line_combo in line_combos:
            xvalues = sorted(list(set([x[x["plot_x_axis"]] for x in experiments])))
            yvalues = []
            ylower = []
            yupper = []
            for xval in xvalues:
                params = {opt : dict(line_combo)[opt] for opt in e0["plot_lines"]}
                params[e0["plot_subplot"]] = subplot_value
                params[e0["plot_x_axis"]] = xval
                tmp = str(e0["plot_x_axis"])
                tmp_map = {"n_routers" : "nw_size", "link_fault_rate" : "link_faults", "router_fault_rate" : "router_faults"}
                tmp = tmp_map.get(tmp, tmp)
                exp_name = "vary_" + tmp + "_" + "_".join([str(params[opt]) for opt in e0["vary"]])
                result = all_results.get(exp_name, None)
                if result is None:
                    yvalues.append(float("nan"))
                    ylower.append(float("nan"))
                    yupper.append(float("nan"))
                else:
                    values = None
                    mean_func = None
                    if metric in ["latency_at_0.1"]:
                        mean_func = hlp.avg_arithmetic # Cost -> Arithmetic mean
                        values = hlp.identify_latencies_at_load(result, 0.1)
                    elif metric in ["zero_load_latency", "average_latency"]:
                        mean_func = hlp.avg_arithmetic # Cost -> Arithmetic mean
                        values = [result[i]["summary"][metric] for i in range(len(result))]
                    elif metric in ["saturation_throughput", "average_throughput", "energy_per_packet"]:
                        mean_func = hlp.avg_harmonic  # Rate -> Harmonic mean
                        values = [result[i]["summary"][metric] for i in range(len(result))]
                    elif metric == "total_power":
                        mean_func = hlp.avg_arithmetic # Cost -> Arithmetic mean
                        values = [result[i]["summary"]["total_router_power"] + result[i]["summary"]["total_link_power"] for i in range(len(result))]
                    else:
                        hlp.print_red("Error: Unknown metric \"%s\" for line plot." % metric)
                        return
                    mean, lower, upper, rel_half_width = hlp.compute_confidence_interval(values, mean_func)
                    yvalues.append(mean)
                    ylower.append(lower)
                    yupper.append(upper)
            line_data.append((xvalues, yvalues, ylower, yupper))
            line_auc.append(hlp.area_under_curve(xvalues, yvalues))
            # Determine the line color
            lab = ""
            if "hashed" in dict(line_combo).get("mod_selection_function",""):
                line_color.append(color_ours)
                if "(PA)ISF" not in labels:
                    labels.append("(PA)ISF")
                    lab = "(PA)ISF"
            elif "lash" in dict(line_combo).get("mod_routing_function",""):
                line_color.append(color_lash)
                if "LASH-TOR" not in labels:
                    labels.append("LASH-TOR")
                    lab = "LASH-TOR"
            elif "deterministic" in dict(line_combo).get("mod_selection_function",""):
                line_color.append(color_det)
                if "DET" not in labels:
                    labels.append("DET")
                    lab = "DET"
            elif "multi" in dict(line_combo).get("routing_function",""):
                line_color.append(color_multi)
                if "Multi" not in labels:
                    labels.append("Multi")
                    lab = "Multi"
            else:
                hlp.print_error("Error: Unknown line type for line plot. line_combo: %s" % str(dict(line_combo)))
            type_label.append(lab)
            line_label.append(" | ".join(cfg.acronyms[opt][dict(line_combo)[opt]].replace(" ","") for opt in e0["plot_lines"] if cfg.acronyms[opt][dict(line_combo)[opt]] is not None))
        # Identify best baseline and best our line and set bold
        best_ours_auc = best_function([line_auc[idx] for idx in range(len(line_auc)) if line_color[idx] == color_ours])
        best_dat_auc = best_function([line_auc[idx] for idx in range(len(line_auc)) if line_color[idx] == color_det])
        best_multi_auc = best_function([line_auc[idx] for idx in range(len(line_auc)) if line_color[idx] == color_multi])
        best_lash_auc = best_function([line_auc[idx] for idx in range(len(line_auc)) if line_color[idx] == color_lash])
        line_bold = [((line_auc[idx] == best_multi_auc) if line_color[idx] == color_multi else (line_auc[idx] == best_dat_auc) if line_color[idx] == color_det else (line_auc[idx] == best_ours_auc) if line_color[idx] == color_ours else (line_auc[idx] == best_lash_auc) if line_color[idx] == color_lash else False) for idx in range(len(line_auc))]
        # Plot all lines
        for bold in [False, True]:
            for idx, (xvalues, yvalues, ylower, yupper) in enumerate(line_data):
                if line_bold[idx] == bold:
                    #line_style = ':' if line_color[idx] == color_multi else ('--' if line_color[idx] == color_det else '-' if line_color[idx] == color_ours else '-.')
                    line_style = "-"
                    marker = 'v' if line_color[idx] == color_multi else ('d' if line_color[idx] == color_det else 'o' if line_color[idx] == color_ours else 's')
                    yerr = [[yvalues[j] - ylower[j] for j in range(len(yvalues))], [yupper[j] - yvalues[j] for j in range(len(yvalues))]] if not all(math.isnan(y) for y in yvalues) else None
                    linewidth, zorder, marker_size = (2.0, 4, 4) if line_bold[idx] else (0.5, 3, 2)
                    ax[i].errorbar(xvalues, yvalues, yerr=yerr, color=line_color[idx], linewidth=linewidth, linestyle=line_style, marker = marker, markersize=marker_size, label=type_label[idx], zorder=zorder, elinewidth = 1, ecolor = hlp.darken_color(line_color[idx], 0.8))
                    if line_bold[idx]:
                        # Annotate line with the label at center of the line (removed to avoid clutter)
                        mid_idx = len(xvalues) // 2
                        # ax[i].text(xvalues[mid_idx], 1.01 * yvalues[mid_idx], line_label[idx], fontsize=8, fontweight='bold', color=line_color[idx], ha='center', va='bottom')
        # Set labels and title
        ax[i].set_title(cfg.full_names[e0["plot_subplot"]][subplot_value])
        ax[i].set_xlabel(cfg.full_names["parameters"][e0["plot_x_axis"]])
        if i == 0:
            ax[i].set_ylabel(metric.replace("_"," ").title())
        ax[i].set_ylim(bottom=0, top = ax[i].get_ylim()[1] * 1.1)
        ax[i].grid(color = "#CCCCCC", linewidth=0.5, zorder=0)
        # Legend with uniform marker size and line width
        if len(set(type_label)) > 2:
            handles, labels = ax[i].get_legend_handles_labels()
            by_label = dict(zip(labels, handles))
            order = ["(PA)ISF","LASH-TOR","DET","Multi"]
            # build proxy legend handles (no errorbars, uniform style)
            legend_handles = []
            for l in order:
                h = by_label[l]
                color = h.lines[0].get_color()   # extract color from errorbar
                marker = h.lines[0].get_marker()
                legend_handles.append(Line2D([0], [0],color=color, marker=marker, linewidth=2, markersize=4))
            ax[i].legend(legend_handles, order, fontsize=6)
        # Special case if all x values are powers of two
        if all(math.log2(x).is_integer() for x in xvalues if x > 0):
            ax[i].set_xscale('log', base=2)
            ax[i].set_xticks(xvalues)
            ax[i].set_xticklabels([str(x) for x in xvalues])
    # Save plot if not all values are NaN
    if do_save and not all(all(math.isnan(yvalues[idx]) for yvalues, _, _, _ in line_data) for idx in range(len(line_data))):
        plt.savefig("plots/line_plot_%s.%s" % (name, cfg.plot_format), dpi=cfg.plot_dpi)
    plt.close()

# Legend for raw data matrix: >=0 Real value, -10 missing in data, -11 not applicable, -12 deadlocked
def create_heatmap_plot(experiments : list[dict], name : str, metric : str, do_save = True) -> None:
    # Config
    digits = int(-math.log10(cfg.default_booksim_config["precision"]))
    # Settings based on metric
    if metric in ["saturation_throughput", "average_throughput"]:
        rank_1_func = np.nanmax
        rank_2_func = lambda l : np.nanmax([float("nan")] + [x for x in l if not math.isnan(x) and x >= 0.0 and x < rank_1_func(l)])
    elif metric in ["latency_at_0.1","zero_load_latency", "energy_per_packet","total_power", "average_latency"]:
        rank_1_func = lambda l : np.nanmin([x for x in l if not math.isnan(x) and x >= 0.0])
        rank_2_func = lambda l : np.nanmin([float("nan")] + [x for x in l if not math.isnan(x) and x >= 0.0 and x > rank_1_func(l)])
    else:
        hlp.print_red("Error: Unknown metric \"%s\" for heatmap plot." % metric)
        return
    # Load all results of this experiment sweep
    all_results = hlp.read_results(experiments)
    # Skip this plot if no results are available
    if len(all_results) == 0:
        hlp.print_yellow("Warning: No results found for heatmap plot \"%s\". Skipping..." % name)
        return (None, None, None)
    hlp.print_cyan("Creating Heatmap plot for \"%s\"" % name)
    # Prepare data gathering
    e0 = experiments[0]
    row_combos = set()
    col_combos = set()
    for exp in experiments:
        row_combo = {opt : exp[opt] for opt in e0["plot_rows"]}
        col_combo = {opt : exp[opt] for opt in e0["plot_cols"]}
        row_combos.add(frozenset(row_combo.items()))
        col_combos.add(frozenset(col_combo.items()))
    # Sort rows and columns based on acronym order
    order = {opt : {key : idx for idx, key in enumerate(cfg.acronyms[opt].keys())} for opt in (experiments[0]["plot_rows"] + experiments[0]["plot_cols"])}
    row_combos = sorted(list(row_combos), key=lambda x: [order[opt][dict(x)[opt]] for opt in experiments[0]["plot_rows"]])
    col_combos = sorted(list(col_combos), key=lambda x: [order[opt][dict(x)[opt]] for opt in experiments[0]["plot_cols"]])
    # Prepare data matrix, initialize with minus one
    data_matrix = np.full((len(row_combos), len(col_combos)), float(-11.0))  # -11 means not applicable (initialize with that)
    # Create plot
    width = 7                               # Fixed, intended of for single column in paper.
    height = 1 + len(row_combos) * 0.125     # Dynamic based on number of rows
    aspect = ((height - 1) / len(row_combos)) / ((width - 2) / len(col_combos))
    fig, ax = plt.subplots(1,1, figsize=(width,height))
    fig.subplots_adjust(top = 0.93, bottom = 0.3, left = 0.18, right = 0.99)
    for exp in experiments:
        # Compute row and column index
        row_combo = frozenset({opt : exp[opt] for opt in e0["plot_rows"]}.items())
        col_combo = frozenset({opt : exp[opt] for opt in e0["plot_cols"]}.items())
        row_idx = row_combos.index(row_combo)
        col_idx = col_combos.index(col_combo)
        # Missing result
        if exp["name"] not in all_results:
            data_matrix[row_idx, col_idx] = -10.0
            continue
        # Fetch result
        result = all_results[exp["name"]]
        if len(result) == 0:
            data_matrix[row_idx, col_idx] = -10.0
            continue
        # Remove deadlocked runs from result
        result = [r for r in result if not any(r["booksim_details"][key].get("number_of_deadlocked_runs", -1) >= r["booksim_config"]["deadlock_attempts"] for key in r["booksim_details"])]
        # If no non-deadlocked runs remain, mark as deadlocked
        if len(result) == 0:
            data_matrix[row_idx, col_idx] = -12.0
            continue
        # Valid result, compute average metric
        if not data_matrix[row_idx, col_idx] == -11.0:
            hlp.print_yellow("Warning: There seem to be two experiments that map to the same heatmap cell. Overwriting previous value.")
        value = None
        if metric in ["saturation_throughput", "latency_at_0.1", "zero_load_latency", "energy_per_packet", "average_throughput", "average_latency"]:
            if metric == "latency_at_0.1":
                values = hlp.identify_latencies_at_load(result, 0.1)
                value = hlp.avg_arithmetic(values)      # Cost -> Arithmetic mean
            else:
                if metric in ["zero_load_latency", "average_latency"]:
                    value = hlp.avg_arithmetic([round(result[i]["summary"][metric],digits) for i in range(len(result))])      # Cost -> Arithmetic mean
                elif metric in ["saturation_throughput", "average_throughput", "energy_per_packet"]:
                    value = hlp.avg_harmonic([round(result[i]["summary"][metric],digits) for i in range(len(result))])        # Rate -> Harmonic mean
        elif metric == "total_power":
            value = hlp.avg_arithmetic([round(result[i]["summary"]["total_router_power"] + result[i]["summary"]["total_link_power"], digits) for i in range(len(result))])      # Cost -> Arithmetic mean
        else:
            hlp.print_red("Error: Unknown metric \"%s\" for heatmap plot." % metric)
            sys.exit(1)
        data_matrix[row_idx, col_idx] = value

    # Create heatmap
    special_colors = [[0.6, 0.6, 0.6, 1.0],[0.6, 0.6, 0.6, 1.0],[0.6, 0.6, 0.6, 1.0],[1.0,1.0,1.0,1.0]]  # Deadlock: red, Not applicable: gray, Missing: dark red, While for safety
    gradient = plt.cm.Blues(np.linspace(0, 0.5, 256))
    all_colors = np.vstack([special_colors, gradient])
    cmap = ListedColormap(all_colors)
    min_for_gradient = min([x for x in data_matrix.flatten() if x >= 0.0 and not math.isnan(x)]) if any(x >= 0.0 and not math.isnan(x) for x in data_matrix.flatten()) else 0.0
     # Define bounds for colors
    log_bounds = np.logspace(np.log10(min_for_gradient), np.log10(np.nanmax(data_matrix)), 257)
    bounds = [-12.5, -11.5, -10.5, -9.5] + list(log_bounds)
    norm = BoundaryNorm(bounds, cmap.N)
    im = ax.imshow(data_matrix, cmap=cmap, norm=norm, aspect=aspect)
    # Set ticks and labels
    ax.set_xticks(np.arange(len(col_combos)))
    ax.set_yticks(np.arange(len(row_combos)))
    ax.set_xticklabels(["|".join(f"{cfg.acronyms[opt][dict(col_combo)[opt]]}" for opt in e0["plot_cols"] if cfg.acronyms[opt][dict(col_combo)[opt]] is not None) for col_combo in col_combos], rotation=45, ha="right", fontsize=8, fontname='monospace')
    ax.set_yticklabels(["|".join(f"{cfg.acronyms[opt][dict(row_combo)[opt]]}" for opt in e0["plot_rows"] if cfg.acronyms[opt][dict(row_combo)[opt]] is not None) for row_combo in row_combos], fontsize=8, fontname='monospace')
    # Loop over data dimensions and create text annotations.
    for i in range(len(row_combos)):
        for j in range(len(col_combos)):
            # Deadlock
            if data_matrix[i, j] == -12.0:
                text = ax.text(j, i, "DL", ha="center", va="center", color="#000000", fontsize=6, fontweight="normal")
            # Not applicable
            elif data_matrix[i, j] == -11.0:
                text = ax.text(j, i, "N/A", ha="center", va="center", color="#000000", fontsize=6, fontweight="normal")
            # Missing
            elif data_matrix[i, j] == -10.0:
                text = ax.text(j, i, "MISS", ha="center", va="center", color="#000000", fontsize=6, fontweight="normal")
            # Real value
            elif not math.isnan(data_matrix[i, j]):
                fw = "bold" if data_matrix[i, j] == rank_1_func(data_matrix[:, j]) else "normal"
                ul = True if data_matrix[i, j] == rank_2_func(data_matrix[:, j]) else False
                #fw = "bold" if data_matrix[i, j] == best_function(data_matrix[:, j]) else "normal"
                val = data_matrix[i, j]
                text_val = str(0 if val == 0 else round(val, 3 - max(0,int(np.floor(np.log10(abs(val)))))))
                if text_val.endswith(".0"):
                    text_val = text_val[:-2]
                if len(text_val) < 5 and "." in text_val:
                    text_val = text_val + (("0" * (5 - len(text_val))))
                text = ax.text(j, i, text_val, ha="center", va="center", color="#000000", fontsize=5.0, fontweight=fw)
                if ul:
                    ax.plot(j, i+0.33, marker="_", color="#000000", markersize=16, markeredgewidth=0.5)
            # NaN value, unclear why this would happen
            else:
                text = ax.text(j, i, "???", ha="center", va="center", color="#000000", fontsize=6, fontweight="normal")

    # Set labels
    ax.set_xlabel(" | ".join([cfg.full_names["parameters"][opt] for opt in e0["plot_cols"]]))
    ax.set_ylabel(" | ".join([cfg.full_names["parameters"][opt] for opt in e0["plot_rows"]]))
    ax.set_title("Heatmap of " + metric.replace("_", " ").title())
    # Save plot
    if do_save:
        plt.savefig("plots/heatmap_%s.%s" % (name, cfg.plot_format), dpi=cfg.plot_dpi)
    plt.close()
    return (data_matrix, row_combos, col_combos)

def visualize_system(adj_list : list[list[int]], latencies : list[list[int]], node_counts : list[int], node_latencies : list[int], router_locations : list[tuple[float,float]], width : float, height : float, name : str, stats_file : str = None, ext_ax = None, title : str = None, exp = None) -> None:
    # Load stats if provided
    link_utilization = {}
    if stats_file is not None:
        stats = hlp.read_json(stats_file)
        link_utilization = {(x["src_router_id"], x["dst_router_id"]): x["packet_count"] for x in stats["links"]}
    combined_utilization = {}
    for (src, dst), util in link_utilization.items():
        if (dst, src) in combined_utilization:
            combined_utilization[(dst, src)] += util
        else:
            combined_utilization[(src, dst)] = util
    max_utilization = max(combined_utilization.values()) if len(combined_utilization) > 0 else 0.0
    # Create plot
    if ext_ax is None:
        fig, ax = plt.subplots(1,1, figsize=(10,10))
        fig.subplots_adjust(top = 0.95, bottom = 0.05, left = 0.05, right = 0.95)
    else:
        ax = ext_ax
    iv = 0.95 # Inverse spacing between components (spacing is 1/iv)
    n = len(router_locations)
    # Plot components as rectangles and routers as circles
    for (i, (x, y)) in enumerate(router_locations):
        col = "#B8D2F5" if node_counts[i] > 0 else "#CCCCCC66"
        rect = plt.Rectangle((x - iv * width/2, y - iv * height/2), iv * width, iv * height, fill=True, color=col, ec='black', zorder=1)
        ax.add_patch(rect)
    for (i, (x, y)) in enumerate(router_locations):
        circ = plt.Circle((x, y), radius=width/8, fill=True, color='skyblue', ec='black', zorder=3)
        ax.add_patch(circ)
        ax.text(x, y, f"{i}", ha='center', va='center', zorder=4, fontsize=8 / (np.sqrt(n) / 8))
    # Links between routers (as arcs to differentiate links that overlap)
    for src_rid in range(len(adj_list)):
        for dst_rid in adj_list[src_rid]:
            # We use bidirectional links, so only draw one direction
            if src_rid > dst_rid:
                (x1,y1) = router_locations[src_rid]
                (x2,y2) = router_locations[dst_rid]
                # Center of circle
                lf = (np.sqrt((x2 - x1)**2 + (y2 - y1)**2) / math.sqrt((width)**2 + (height)**2))
                af = 100 if lf <= 1 else lf / 2
                xdir, ydir = x2-x1, y2-y1
                ndir = np.sqrt(xdir**2 + ydir**2)
                vx, vy = (-ydir / ndir, xdir / ndir)
                xx, yy = ((x1 + x2) / 2) + af * ndir * vx, ((y1 + y2) / 2) + af * ndir * vy
                # Parameters for the circular arc
                radius = np.sqrt((x1 - xx)**2 + (y1 - yy)**2)  # Radius of the circle
                angle_start = np.arctan2(y1 - yy, x1 - xx)
                angle_end = np.arctan2(y2 - yy, x2 - xx)
                if angle_end < angle_start:
                    angle_end += 2 * np.pi
                # Define the arc
                theta = np.linspace(angle_start, angle_end, 100)
                x_arc = xx + radius * np.cos(theta)
                y_arc = yy + radius * np.sin(theta)
                # Define color based on utilization if available: From Black (low) to Red (high)
                col = "#000000"
                lwd = 1
                util = 0.0
                if (src_rid, dst_rid) in link_utilization:
                    util = link_utilization[(src_rid, dst_rid)]
                if (dst_rid, src_rid) in link_utilization:
                    util += link_utilization[(dst_rid, src_rid)]
                if util > 0.0:
                    norm_util = min((util / max_utilization if max_utilization > 0 else 0.0), 1.0)
                    r = int(255 * norm_util)
                    g = int(0)
                    b = int(0)
                    col = f"#{r:02X}{g:02X}{b:02X}"
                    lwd = 3
                # Plot the arc
                ax.plot(x_arc, y_arc, lw=lwd, color = col, zorder=2)
    # Adjust plot limits
    ax.autoscale_view()
    ax.set_aspect('equal', adjustable='box')
    if title is not None:
        ax.set_title(title)
    # Save plot
    if ext_ax is None:
        plt.savefig("plots/system_visualization_%s.%s" % (name, cfg.plot_format), dpi=cfg.plot_dpi)
    plt.close()

def visualize_distinct_systems(experiments : dict[list[dict]], name : str, stats_file : str = None) -> None:
    distinct_system_names = []
    for experiment in experiments:
        exp = SimpleNamespace(**experiment)
        distinct_system_name = "_".join(str(x) for x in [exp.n_routers, exp.topology, exp.nodes_per_router, exp.width, exp.height])
        if distinct_system_name not in distinct_system_names:
            distinct_system_names.append(distinct_system_name)
            network = gn.generate_network(exp.n_routers, exp.topology, exp.nodes_per_router, exp.link_latency_function, exp.node_link_latency, exp.width, exp.height)
            # Insert faults into the network if configured
            if exp.link_fault_rate > 0.0:
                network = fi.insert_link_faults(network, exp.link_fault_rate, cfg.fault_random_seed, cfg.fault_attempts)
                if network is None:
                    hlp.print_red(f"Failed to insert enough link faults for experiment {exp.name}. Skipping...")
            if exp.router_fault_rate > 0.0:
                network = fi.insert_router_faults(network, exp.router_fault_rate, cfg.fault_random_seed, cfg.fault_attempts)
                if network is None:
                    hlp.print_red(f"Failed to insert enough router faults for experiment {exp.name}. Skipping...")
            nw = SimpleNamespace(**network)
            visualize_system(nw.adj_list, nw.latencies, nw.node_counts, nw.node_latencies, nw.router_locations, exp.width, exp.height, name + "-" + distinct_system_name, stats_file, exp = exp)

def visualize_utilization_for_heatmap_experiment(experiments : list[dict], name : str, metric : str, do_save = True) -> None:
    # Load all results of this experiment sweep
    all_results = hlp.read_results(experiments)
    # Skip this plot if no results are available
    if len(all_results) == 0:
        hlp.print_yellow("Warning: No results found for heatmap plot \"%s\". Skipping..." % name)
        return (None, None, None)
    hlp.print_cyan("Creating Heatmap-Style System visualizations for \"%s\"" % name)
    # Prepare data gathering
    e0 = experiments[0]
    row_combos = set()
    col_combos = set()
    for exp in experiments:
        row_combo = {opt : exp[opt] for opt in e0["plot_rows"]}
        col_combo = {opt : exp[opt] for opt in e0["plot_cols"]}
        row_combos.add(frozenset(row_combo.items()))
        col_combos.add(frozenset(col_combo.items()))
    # Sort rows and columns based on acronym order
    order = {opt : {key : idx for idx, key in enumerate(cfg.acronyms[opt].keys())} for opt in (experiments[0]["plot_rows"] + experiments[0]["plot_cols"])}
    row_combos = sorted(list(row_combos), key=lambda x: [order[opt][dict(x)[opt]] for opt in experiments[0]["plot_rows"]])
    col_combos = sorted(list(col_combos), key=lambda x: [order[opt][dict(x)[opt]] for opt in experiments[0]["plot_cols"]])
    n_rows = len(row_combos)
    n_cols = len(col_combos)
    # Create plot
    fig, ax = plt.subplots(n_rows,n_cols, figsize=(10 * n_cols,10 * n_rows))
    fig.subplots_adjust(top = 1.0, bottom = 0.0, left = 0.0, right = 1.0)
    for exp in experiments:
        # Compute row and column index
        row_combo = frozenset({opt : exp[opt] for opt in e0["plot_rows"]}.items())
        col_combo = frozenset({opt : exp[opt] for opt in e0["plot_cols"]}.items())
        row_idx = row_combos.index(row_combo)
        col_idx = col_combos.index(col_combo)
        # Missing result
        result = all_results.get(exp["name"], None)
        sat_tp = result[-1]["summary"]["saturation_throughput"] if result is not None and len(result) > 0 else float("nan")
        # Construct network
        exp_ = SimpleNamespace(**exp)
        network = gn.generate_network(exp_.n_routers, exp_.topology, exp_.nodes_per_router, exp_.link_latency_function, exp_.node_link_latency, exp_.width, exp_.height)
        filename = "booksim2/src/stats/%s_%s.csv" % (exp_.name, "%.6f" % sat_tp)
        stats_file = filename if os.path.exists(filename) else None
        # Insert faults into the network if configured
        if exp_.link_fault_rate > 0.0:
            network = fi.insert_link_faults(network, exp_.link_fault_rate, cfg.fault_random_seed, cfg.fault_attempts)
            if network is None:
                hlp.print_red(f"Failed to insert enough link faults for exp_eriment {exp_.name}. Skipping...")
        if exp_.router_fault_rate > 0.0:
            network = fi.insert_router_faults(network, exp_.router_fault_rate, cfg.fault_random_seed, cfg.fault_attempts)
            if network is None:
                hlp.print_red(f"Failed to insert enough router faults for exp_eriment {exp_.name}. Skipping...")
        nw = SimpleNamespace(**network)
        title = " | ".join([f"{cfg.acronyms[opt][dict(row_combo)[opt]]}" for opt in e0["plot_rows"] if cfg.acronyms[opt][dict(row_combo)[opt]] is not None]) + " | " + " | ".join([f"{cfg.acronyms[opt][dict(col_combo)[opt]]}" for opt in e0["plot_cols"] if cfg.acronyms[opt][dict(col_combo)[opt]] is not None])
        visualize_system(nw.adj_list, nw.latencies, nw.node_counts, nw.node_latencies, nw.router_locations, exp_.width, exp_.height, name, stats_file, ax = ax[row_idx][col_idx], title = title, exp = exp)
    plt.savefig("plots/utilization_visualization_%s.%s" % (name, cfg.plot_format), dpi=cfg.plot_dpi)
    plt.close()


def plot_stats_histograms(stats_file : str, name : str) -> None:
    # Load stats
    stats = hlp.read_json(stats_file)
    packets_per_link = [x["packet_count"] for x in stats["links"]]
    routing_choices_phys_sum = sum(x["count"] for x in stats["routing_choices_physical"])
    routing_choices_phys = {x["n_choices"] : 100 * x["count"] / routing_choices_phys_sum for x in stats["routing_choices_physical"]} if routing_choices_phys_sum > 0 else {}
    rouiing_choices_phys = {n : (routing_choices_phys[n] if n in routing_choices_phys else 0.0) for n in range(max(routing_choices_phys.keys()) + 1)}  # Ensure all choice counts up to max are represented, even if zero
    routing_choices_virt_sum = sum(x["count"] for x in stats["routing_choices_virtual"])
    routing_choices_virt = {x["n_choices"] : 100 * x["count"] / routing_choices_virt_sum for x in stats["routing_choices_virtual"]} if routing_choices_virt_sum > 0 else {}
    rouiing_choices_virt = {n : (routing_choices_virt[n] if n in routing_choices_virt else 0.0) for n in range(max(routing_choices_virt.keys()) + 1)}  # Ensure all choice counts up to max are represented, even if zero
    # Plot histogram of packets per link
    fig, ax = plt.subplots(1,3, figsize=(9, 3))   
    fig.subplots_adjust(top = 0.9, bottom = 0.2, left = 0.075, right = 0.98, wspace=0.3)
    ax[0].hist(packets_per_link, bins=20, color = cfg.colors[0])
    ax[0].set_title("Packets per Link")
    ax[0].set_xlabel("Packet Count")
    ax[0].set_ylabel("Number of Links")
    ax[0].set_xlim(left=0)
    # Plot histogram of routing choices physical and virtual
    for (i, name_, data) in [(1, "Physical", routing_choices_phys), (2, "Virtual", routing_choices_virt)]: 
        ax[i].bar(data.keys(), data.values(), color=cfg.colors[i])
        ax[i].set_title("Routing Choices (%s)" % name_)
        ax[i].set_xlabel("Number of Choices")
        ax[i].set_ylabel("Percentage of Occurrences [%]")
        ax[i].set_xticks(range(max(data.keys()) + 1))
        ax[i].set_xlim(0.5, max(data.keys()) + 0.5)
    # Save plot
    plt.savefig("plots/stats_histograms_%s.%s" % (name, cfg.plot_format), dpi=cfg.plot_dpi)
    plt.close()
    



if __name__ == "__main__":
    # If he option -s or --stats is provided use the following argument to load stats
    # In that case, we just visualize the corresponding system and no not generate all plots
    if "-s" in sys.argv or "--stats" in sys.argv:
        stats_file = None
        if "-s" in sys.argv:
            idx = sys.argv.index("-s")
            if idx + 1 < len(sys.argv):
                stats_file = sys.argv[idx + 1]
        elif "--stats" in sys.argv:
            idx = sys.argv.index("--stats")
            if idx + 1 < len(sys.argv):
                stats_file = sys.argv[idx + 1]
        # Find the right experiment configuration based on the file name
        matched_experiment = None
        for exp_name, experiments in experiment_sweeps.items():
            for exp in experiments:
                if exp["name"] in stats_file:
                    matched_experiment = exp
                    break
            if matched_experiment is not None:
                break
        if matched_experiment is None:
            hlp.print_red("Error: Could not find matching experiment configuration for stats file \"%s\"" % stats_file)
            sys.exit(1)
        else:
            # We create two plots, one that visualizes the system with link utilizations and one with histograms of various stats.
            visualize_distinct_systems([matched_experiment], "utilization_visualization", stats_file)
            plot_stats_histograms(stats_file, matched_experiment["name"])
    # No utilization file provided, generate all plots
    else:
        # Compose experiments based on configuration
        for exp_name, experiments in experiment_sweeps.items():
            hlp.print_magenta(f"Generating plots for experiment: {exp_name}")
            if experiments[0]["plot_type"] == "lines":
                for metric in ["saturation_throughput", "latency_at_0.1", "zero_load_latency", "energy_per_packet"]:
                    plot_name = exp_name + "_" + metric
                    # Create line plot
                    create_line_plots(experiments, plot_name, metric)
            elif experiments[0]["plot_type"] == "heatmap":
                for metric in ["saturation_throughput", "latency_at_0.1", "zero_load_latency", "energy_per_packet"]:
                    plot_name = exp_name + "_" + metric
                    # Create heatmap plot 
                    create_heatmap_plot(experiments, plot_name, metric)
                    # Create improvement bar char
                    plot_improvement_bar_chart(experiments, plot_name, metric, "us_vs_baseline")
                    plot_improvement_bar_chart(experiments, plot_name, metric, "paisf_vs_isf")
                    # This create a very large PDF/PNG with a grid of all visualizations. 
                    # visualize_utilization_for_heatmap_experiment(experiments, plot_name, metric)
            elif experiments[0]["plot_type"] == "heatmap_trace":
                for metric in ["average_throughput", "average_latency", "energy_per_packet"]:
                    plot_name = exp_name + "_" + metric
                    # Create heatmap plot 
                    create_heatmap_plot(experiments, plot_name, metric)
                    # Create improvement bar char
                    plot_improvement_bar_chart(experiments, plot_name, metric, "us_vs_baseline")
                    plot_improvement_bar_chart(experiments, plot_name, metric, "paisf_vs_isf")
                    # Create adaptive map size.
            # Visualize distinct systems (do not create multiple plots for same system with different routing or traffic)
            # visualize_distinct_systems(experiments, "all") # TODO: Add back for final version

