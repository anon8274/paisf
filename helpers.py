# Python modules
import os
import sys
import csv
import json
import inspect
import builtins
import numpy as np
from collections import deque
from scipy import integrate
import threading

# Custom modules
import config as cfg

# Thread-safe mapping from real thread IDs to sequential TIDs
# Used for printing with thread identification

_tid_map = {}
_tid_lock = threading.Lock()
_next_tid = 0

def get_tid() -> int:
    global _next_tid
    tid = threading.get_ident()
    with _tid_lock:
        if tid not in _tid_map:
            _tid_map[tid] = _next_tid
            _next_tid += 1
        return _tid_map[tid]

def print(txt: str, end: str = "\n") -> None:
    tid = get_tid()
    with _tid_lock:
        # if only one thread mapped so far, omit the prefix
        if len(_tid_map) == 1:
            builtins.print(txt, end=end)
        else:
            builtins.print(f"[{tid}] {txt}", end=end)

def red(txt : str) -> str:
    return "\33[31m%s\033[0m" % txt

def green(txt :str) -> str:
    return "\33[32m%s\033[0m" % txt 

def magenta(txt : str) -> str:
    return "\33[35m%s\033[0m" % txt

def blue(txt : str) -> str:
    return "\33[34m%s\033[0m" % txt

def yellow(txt : str) -> str:
    return "\33[33m%s\033[0m" % txt

def cyan(txt : str) -> str:
    return "\33[36m%s\033[0m" % txt

def print_error(txt : str, end: str = "\n") -> None:
    frame = inspect.stack()[1]
    caller_file = frame.filename.split('/')[-1]
    caller_line = frame.lineno
    caller_func = frame.function
    print(red(f"ERROR in {caller_file}:{caller_line} in {caller_func}(): {txt}"))

def print_red(txt : str, end: str = "\n") -> None:
    print(red(txt), end=end)

def print_green(txt :str, end: str = "\n") -> None:
    print(green(txt), end=end)

def print_magenta(txt : str, end: str = "\n") -> None:
    print(magenta(txt), end=end)

def print_blue(txt : str, end: str = "\n") -> None:
    print(blue(txt), end=end)

def print_yellow(txt : str, end: str = "\n") -> None:
    print(yellow(txt), end=end)

def print_cyan(txt : str, end: str = "\n") -> None:
    print(cyan(txt), end=end)

def is_float(value):
    try:
        float(value)
        return True
    except ValueError:
        return False

def write_json(filename, content):
    try:
        file = open(filename, "w")
        file.write(json.dumps(content, indent=4))
        file.close()
        return
    except Exception as e:
        print("Error writing file: %s/%s -> %s" % (os.getcwd(), filename, e))
        sys.exit(1)

def read_json(filename, suppress_errors=False):
    try:
        file = open(filename, "r")
        file_content = json.loads(file.read())
        file.close()
        return file_content
    except Exception as e:
        if not suppress_errors:
            print("Error reading file: %s/%s -> %s" % (os.getcwd(), filename, e))
        sys.exit(1)

def manhattan_distance(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])

def read_results(experiments, suppress_output=False):
    # maps experiment names to their results
    results = {}
    for experiment in experiments:
        result_file = "results/results_%s.json" % experiment["name"]
        if os.path.isfile(result_file):
            results[experiment["name"]] = read_json(result_file, suppress_errors=True)
        else:
            if not suppress_output:
                print_red("Warning: result file for experiment \"%s\" not found." % experiment["name"])
    return results

def avg_arithmetic(values : list) -> float:
    if len(values) == 0:
        return float("nan")
    return sum(values) / len(values)

def avg_harmonic(values : list) -> float:
    if len(values) == 0:
        return float("nan")
    reciprocal_sum = sum(1.0 / v for v in values if v != 0)
    return len(values) / reciprocal_sum if reciprocal_sum != 0 else 0.0

def avg_geometric(values : list) -> float:
    if len(values) == 0:
        return float("nan")
    product = 1.0
    for v in values:
        product *= v
    return product ** (1.0 / len(values))

def read_csv(filename : str, delimiter : str = ",") -> list:
    data = []
    try:
        with open(filename, 'r') as csvfile:
            csvreader = csv.reader(csvfile, delimiter=delimiter)
            for row in csvreader:
                data.append(row)
    except Exception as e:
        print_error(f"Error reading CSV file {filename}: {e}")
    return data


def area_under_curve(xvalues, yvalues):
    assert len(xvalues) == len(yvalues)
    assert len(xvalues) >= 2

    area = 0.0
    for i in range(len(xvalues) - 1):
        dx = xvalues[i + 1] - xvalues[i]
        area += 0.5 * (yvalues[i] + yvalues[i + 1]) * dx
    return area

def identify_latencies_at_load(all_results, load):
    latencies = []
    for results in all_results:
        if str(load) not in results["booksim_details"].keys():
            continue
        elif results["booksim_details"][str(load)]["status"] != "Success":
            continue
        elif "packet_latency" not in results["booksim_details"][str(load)].keys():
            continue
        elif "avg" not in results["booksim_details"][str(load)]["packet_latency"].keys():
            continue
        else:
            latencies.append(results["booksim_details"][str(load)]["packet_latency"]["avg"])
    return latencies


def split_string(text, k):
    words = text.split()
    total_len = sum(len(w) for w in words) + max(len(words) - 1, 0)  # incl. spaces
    target = total_len / k

    parts = []
    current = []
    current_len = 0

    for w in words:
        add_len = len(w) if not current else len(w) + 1  # space if needed

        # decide: add to current or start new part
        if current and len(parts) < k - 1:
            if abs((current_len + add_len) - target) > abs(current_len - target):
                parts.append(" ".join(current))
                current = [w]
                current_len = len(w)
                continue

        current.append(w)
        current_len += add_len

    parts.append(" ".join(current))

    # if fewer than k parts (edge case), pad with empty strings
    while len(parts) < k:
        parts.append("")

    return "\n".join(parts)

def compute_network_diameter(adj: list[list[int]]) -> int:
    def bfs_longest_distance(start: int) -> int:
        visited = [-1] * len(adj)
        visited[start] = 0
        q = deque([start])
        max_dist = 0
        
        while q:
            u = q.popleft()
            for v in adj[u]:
                if visited[v] == -1:
                    visited[v] = visited[u] + 1
                    max_dist = max(max_dist, visited[v])
                    q.append(v)
        return max_dist

    return max(bfs_longest_distance(i) for i in range(len(adj)))

def compute_confidence_interval(values : list[float], mean_func):
    alpha = 1.0 - cfg.ci_confidence_level
    bootstrap_means = []
    for _ in range(cfg.ci_num_bootstrap_samples):
        sample = np.random.choice(values, size=len(values), replace=True)
        bootstrap_means.append(mean_func(sample))
    # Compute percentiles for CI
    lower = np.percentile(bootstrap_means, 100 * (alpha/2))
    upper = np.percentile(bootstrap_means, 100 * (1 - alpha/2))
    mean = mean_func(values)
    rel_half_width = ((upper - lower) / 2) / mean
    # return results
    return (mean, lower, upper, rel_half_width)



def flatten_list(nested_list : list[list]) -> list:
    flat_list = []
    for item in nested_list:
        if isinstance(item, list):
            flat_list.extend(flatten_list(item))
        else:
            flat_list.append(item)
    return flat_list

def darken_color(color: str, factor: float) -> str:
    color = color.lstrip('#')
    r = int(color[0:2], 16)
    g = int(color[2:4], 16)
    b = int(color[4:6], 16)
    r = max(0, min(255, int(r * factor)))
    g = max(0, min(255, int(g * factor)))
    b = max(0, min(255, int(b * factor)))
    return f'#{r:02x}{g:02x}{b:02x}'
