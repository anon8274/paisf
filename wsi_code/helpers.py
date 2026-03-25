# Python modules
import os
import sys
import math
import json
import inspect
import numpy as np
from scipy import integrate

def register_error(msg : str) -> None:
    frame = inspect.stack()[1]
    caller_file = frame.filename.split('/')[-1]
    caller_line = frame.lineno
    caller_func = frame.function
    print_red(f"ERROR in {caller_file}:{caller_line} in {caller_func}(): {msg}")
    sys.exit(1)

def pair_closest(points):
    points = points[:]  # copy
    pairs = []

    def dist(p, q):
        return math.hypot(p[0] - q[0], p[1] - q[1])

    while len(points) > 1:
        min_d = float("inf")
        best = (0, 1)  # initialize with first valid pair

        for i in range(len(points)):
            for j in range(i + 1, len(points)):
                d = dist(points[i], points[j])
                if d < min_d:
                    min_d = d
                    best = (i, j)

        # extract and store
        p1, p2 = points[best[0]], points[best[1]]
        pairs.append((p1, p2))

        # remove both points (pop higher index first)
        for idx in sorted(best, reverse=True):
            points.pop(idx)

    # For odd number of points, add the last one with itself
    if len(points) == 1:
        pairs.append((points[0], points[0]))

    return pairs

def darken(hex_color, factor=0.7):
    """
    Darken a hex color by the given factor (0–1).
    factor=0.7 → 30% darker
    """
    # remove leading '#'
    hex_color = hex_color.lstrip('#')

    # convert hex to RGB ints
    r = int(hex_color[0:2], 16)
    g = int(hex_color[2:4], 16)
    b = int(hex_color[4:6], 16)

    # apply darkening
    r = int(r * factor)
    g = int(g * factor)
    b = int(b * factor)

    # clamp to [0,255] and format back to hex
    return f"#{r:02x}{g:02x}{b:02x}"

def compute_manhattan_distance(point1 : tuple[float, float], point2 : tuple[float, float]) -> float:
    return abs(point1[0] - point2[0]) + abs(point1[1] - point2[1])

