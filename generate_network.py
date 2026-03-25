# Import python modules
from typing import Callable
import math
import sys

# Import custom modules
import helpers as hlp
from wsi_code.System import construct_system

def add_bidirectional_link(adj_list : list[list[int]], lengths : list[list[float]], latencies : list[list[int]], rid1 : int, rid2 : int, length : float, latency : int):
    # Forward link
    adj_list[rid1].append(rid2)
    lengths[rid1].append(length)
    latencies[rid1].append(latency)
    # Backward link
    adj_list[rid2].append(rid1)
    lengths[rid2].append(length)
    latencies[rid2].append(latency)

##########################################################################################
# Mesh
##########################################################################################

# Function to generate a 2D mesh network
# Router IDs are assigned in row-major order
def generate_mesh_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float) -> dict:
    # Validate inputs: For a 2D mesh, n_routers must be a perfect square
    sqrt_n = int(n_routers ** 0.5)
    if sqrt_n * sqrt_n != n_routers:
        hlp.print_error("ERROR: For a mesh topology, the number of routers must be a perfect square.")
        sys.exit(1)
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = [((col + 0.5) * width, (row + 0.5) * height) for row in range(sqrt_n) for col in range(sqrt_n)]
    # Generate topology
    for row in range(sqrt_n):
        for col in range(sqrt_n):
            rid = row * sqrt_n + col
            # Connect to left neighbor
            if col > 0:
                other_rid = rid - 1
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Connect to bottom neighbor
            if row > 0:
                other_rid = rid - sqrt_n
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

##########################################################################################
# Torus
##########################################################################################

# Function to generate a 2D torus network
# Router IDs are assigned in row-major order
def generate_torus_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float) -> dict:
    # Validate inputs: For a 2D torus, n_routers must be a perfect square
    sqrt_n = int(n_routers ** 0.5)
    if sqrt_n * sqrt_n != n_routers:
        hlp.print_error("ERROR: For a torus topology, the number of routers must be a perfect square.")
        sys.exit(1)
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = [((col + 0.5) * width, (row + 0.5) * height) for row in range(sqrt_n) for col in range(sqrt_n)]
    # Generate topology
    for row in range(sqrt_n):
        for col in range(sqrt_n):
            rid = row * sqrt_n + col
            # Connect to left neighbor (with wrap-around)
            other_rid = row * sqrt_n + ((col - 1) % sqrt_n)
            link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
            add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Connect to bottom neighbor (with wrap-around)
            other_rid = ((row - 1) % sqrt_n) * sqrt_n + col
            link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
            add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

##########################################################################################
# Folded Torus
##########################################################################################

# Function to generate a 2D folded torus network
def generate_folded_torus_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float) -> dict:
    # Validate inputs: For a 2D folded torus, n_routers must be a perfect square
    sqrt_n = int(n_routers ** 0.5)
    if sqrt_n * sqrt_n != n_routers:
        hlp.print_error("ERROR: For a folded torus topology, the number of routers must be a perfect square.")
        sys.exit(1)
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = [((col + 0.5) * width, (row + 0.5) * height) for row in range(sqrt_n) for col in range(sqrt_n)]
    # Generate topology
    for row in range(sqrt_n):
        for col in range(sqrt_n):
            rid = row * sqrt_n + col
            # Connect to left neighbor
            if col > 0:
                other_rid = row * sqrt_n + ((col - 2) if col >= 2 else (col - 1))
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Connect to bottom neighbor (with wrap-around)
            if row > 0:
                other_rid = ((row - 2) if row >= 2 else (row - 1)) * sqrt_n + col
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
    # Generate the one-hop link at end of rows and columns
    for i in range(sqrt_n):
        # Horizontal
        rid1 = i * sqrt_n + (sqrt_n - 2)
        rid2 = i * sqrt_n + (sqrt_n - 1)
        link_len = hlp.manhattan_distance(router_locations[rid1], router_locations[rid2])
        add_bidirectional_link(adj_list, lengths, latencies, rid1, rid2, link_len, link_latency_function(link_len))
        # Column wrap-around
        rid1 = (sqrt_n - 2) * sqrt_n + i
        rid2 = (sqrt_n - 1) * sqrt_n + i
        link_len = hlp.manhattan_distance(router_locations[rid1], router_locations[rid2])
        add_bidirectional_link(adj_list, lengths, latencies, rid1, rid2, link_len, link_latency_function(link_len))
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

##########################################################################################
# Flattened Butterfly
##########################################################################################

# Function to generate a butterfly network
def generate_flattened_butterfly_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float) -> dict:
    # Validate inputs: For a butterfly network, n_routers must be a perfect square
    sqrt_n = int(n_routers ** 0.5)
    if sqrt_n * sqrt_n != n_routers:
        hlp.print_error("ERROR: For a butterfly topology, the number of routers must be a perfect square.")
        sys.exit(1)
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = [((col + 0.5) * width, (row + 0.5) * height) for row in range(sqrt_n) for col in range(sqrt_n)]
    # Generate topology
    for row in range(sqrt_n):
        for col in range(sqrt_n):
            rid = row * sqrt_n + col
            # Connect to all routers in the same row
            for other_col in range(sqrt_n):
                if other_col > col:
                    other_rid = row * sqrt_n + other_col
                    link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                    add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Connect to all routers in the same column
            for other_row in range(sqrt_n):
                if other_row > row:
                    other_rid = other_row * sqrt_n + col
                    link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                    add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

##########################################################################################
# HexaMesh
##########################################################################################

# Function to generate a HexaMesh network
def generate_hexamesh_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float) -> dict:
    # Check if the chiplet count is a valid HexaMesh size (works up to 29.7k routers)
    cnt_to_rad = {3 * r**2 + 3 * r + 1: r for r in range(100)} 
    if n_routers not in cnt_to_rad:
        hlp.print_error("ERROR: For a HexaMesh topology, the number of routers must be a valid HexaMesh size.")
        hlp.print_error("       Supported sizes are: %s" % (", ".join([str(k) for k in cnt_to_rad.keys()])))
        sys.exit(1)
    radius = cnt_to_rad[n_routers]
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = []
    # Generate topology
    rows = 2 * radius + 1
    routers_per_row = list(range(radius + 1, 2 * radius + 1, 1)) + list(range(2 * radius + 1, radius, -1))
    row_start_ids = [sum(routers_per_row[:r]) for r in range(rows)]
    row_end_ids = [sum(routers_per_row[:r + 1]) - 1 for r in range(rows)]
    # Assign router locations first before adding links
    for row in range(rows):
        for col in range(routers_per_row[row]):
            x_offset = width * (radius - (routers_per_row[row] - 1) / 2)
            x = x_offset + col * width
            y = height * row
            router_locations.append((x, y))
    # Add links
    for row in range(rows):
        for col in range(routers_per_row[row]):
            rid = row_start_ids[row] + col
            # Horizontal links
            if col < routers_per_row[row] - 1:
                other_rid = rid + 1
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Diagonal links of type '/'
            if (row < radius) or (row >= radius and row < rows - 1 and col > 0):
                shift = 0 if row < radius else -1
                other_rid = row_start_ids[row + 1] + col + shift
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Diagonal links of type '\'
            if (row < radius) or (row >= radius and row < rows - 1 and col < (routers_per_row[row] - 1)):
                shift = 1 if row < radius else 0
                other_rid = row_start_ids[row + 1] + col + shift
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

##########################################################################################
# FoldedHexaTorus
##########################################################################################

# Function to generate a FoldedHexaTorus network
def generate_foldedhexatorus_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float) -> dict:
    # Check if the chiplet count is a valid FoldedHexaTorus size (works up to 29.7k routers)
    cnt_to_rad = {3 * r**2 + 3 * r + 1: r for r in range(100)} 
    if n_routers not in cnt_to_rad:
        hlp.print_error("ERROR: For a HexaMesh topology, the number of routers must be a valid HexaMesh size.")
        hlp.print_error("       Supported sizes are: %s" % (", ".join([str(k) for k in cnt_to_rad.keys()])))
        sys.exit(1)
    radius = cnt_to_rad[n_routers]
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = []
    # Generate topology
    rows = 2 * radius + 1
    routers_per_row = list(range(radius + 1, 2 * radius + 1, 1)) + list(range(2 * radius + 1, radius, -1))
    row_start_ids = [sum(routers_per_row[:r]) for r in range(rows)]
    row_end_ids = [sum(routers_per_row[:r + 1]) - 1 for r in range(rows)]
    # Assign router locations and add links
    for row in range(rows):
        for col in range(routers_per_row[row]):
            x_offset = width * (radius - (routers_per_row[row] - 1) / 2)
            x = x_offset + col * width
            y = height * row
            router_locations.append((x, y))
    # Add links
    for row in range(rows):
        for col in range(routers_per_row[row]):
            rid = row_start_ids[row] + col
            # Horizontal links: One-hop link at start of row
            if col == 0 and routers_per_row[row] > 1:
                other_rid = rid + 1
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Horizontal links: Regular links with stride 2
            if col + 2 < routers_per_row[row]:
                other_rid = rid + 2
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Horizontal links: One-hop link at end of row
            if col == routers_per_row[row] - 1 and routers_per_row[row] > 1:
                other_rid = rid - 1
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Diagonal links of type '/'
            if (row < rows - 2) and ((col < routers_per_row[row] - 2) or (row + 2 <= radius) or (row == radius-1 and col == routers_per_row[row] - 2)):
                shift = 0 if row >= radius else (1 if row == radius-1 else 2)
                other_rid = row_start_ids[row + 2] + col + shift
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Diagonal Corner-case links of type '/' (top-right)
            if (row >= radius) and ((col == routers_per_row[row] - 1) or (row == 2*radius)):
                shift = -1 if row == radius else 0
                other_rid = row_start_ids[row - 1] + col + shift
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Diagonal Corner-case links of type '/' (bottom-left)
            if (row <= radius) and ((col == 0) or (row == 0)):
                shift = 0 if row == radius else 1
                other_rid = row_start_ids[row + 1] + col + shift
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Diagonal links of type '\'
            if (row < rows - 2) and ((col > 1) or (row + 2 <= radius) or (row == radius-1 and col == 1)):
                shift = 0 if row + 2 <= radius else (-1 if row == radius-1 else -2)
                other_rid = row_start_ids[row + 2] + col + shift
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Diagonal Corner-case links of type '\' (top-left)
            if (row >= radius) and ((col == 0) or (row == 2*radius)):
                shift = 0 if row == radius else 1
                other_rid = row_start_ids[row - 1] + col + shift
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
            # Diagonal Corner-case links of type '\' (bottom-right)
            if (row <= radius) and ((col == routers_per_row[row] - 1) or (row == 0)):
                shift = -1 if row == radius else 0
                other_rid = row_start_ids[row + 1] + col + shift
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

##########################################################################################
# Hypercube
##########################################################################################

# Function to generate a hypercube network
def generate_hypercube_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float) -> dict:
    # Validate inputs: For a hypercube, n_routers must be a square number and the square root must be a power of 2
    sqrt_n = int(n_routers ** 0.5)
    log2_sqrt_n = int(math.log2(sqrt_n))
    if sqrt_n * sqrt_n != n_routers or 2 ** log2_sqrt_n != sqrt_n:
        hlp.print_error("ERROR: For a hypercube topology, the number of routers must be a perfect square and the square root must be a power of 2.")
        sys.exit(1)
    # Function to compute hamming distance, used to determine connections
    def compute_hamming_distance(a, b):
        hdist = bin(a ^ b).count("1")
        abin, bbin = bin(a)[2:], bin(b)[2:]
        nbit = max(len(abin), len(bbin))
        abin, bbin = abin.zfill(nbit), bbin.zfill(nbit)
        diff = [i for i in range(1,nbit+1) if abin[-i] != bbin[-i]]
        return hdist, diff
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = [((col + 0.5) * width, (row + 0.5) * height) for row in range(sqrt_n) for col in range(sqrt_n)]
    # Generate topology
    for src_rid in range(n_routers):
        row = src_rid // sqrt_n
        col = src_rid % sqrt_n
        for dst_rid in range(n_routers):
            hamming_distance, _ = compute_hamming_distance(src_rid, dst_rid)
            if hamming_distance == 1:
                link_len = hlp.manhattan_distance(router_locations[src_rid], router_locations[dst_rid])
                add_bidirectional_link(adj_list, lengths, latencies, src_rid, dst_rid, link_len, link_latency_function(link_len))
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

##########################################################################################
# Double Butterfly
##########################################################################################

# Function to generate a double butterfly network
def generate_double_butterfly_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float) -> dict:
    # Validate inputs: For a double butterfly, n_routers must be a square number and the square root must also be a power of 2
    sqrt_n = int(n_routers ** 0.5)
    log2_sqrt_n = int(math.log2(sqrt_n))
    if sqrt_n * sqrt_n != n_routers or 2 ** log2_sqrt_n != sqrt_n:
        hlp.print_error("ERROR: For a double butterfly topology, the number of routers must be a perfect square and the square root must be a power of 2.")
        sys.exit(1)
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = [((col + 0.5) * width, (row + 0.5) * height) for row in range(sqrt_n) for col in range(sqrt_n)]
    # Generate topology
    # Horizontal links
    for row in range(sqrt_n):
        for col in range(sqrt_n):
            # Router location
            if col < sqrt_n - 1:
                rid = row * sqrt_n + col
                other_rid = rid + 1
                link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
    # Diagonal links
    for stage in range(log2_sqrt_n):
        for col1 in range(2**stage-1, sqrt_n, 2**(stage+1)):
            col2 = col1 + 1
            for row1 in range(sqrt_n):
                row2 = ((row1 // 2**(stage+1)) * 2**(stage+1)) + ((row1 + 2**stage) % 2**(stage+1))
                rid1 = row1 * sqrt_n + col1
                rid2 = row2 * sqrt_n + col2
                link_len = hlp.manhattan_distance(router_locations[rid1], router_locations[rid2])
                add_bidirectional_link(adj_list, lengths, latencies, rid1, rid2, link_len, link_latency_function(link_len))
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

##########################################################################################
# ButterDonut
##########################################################################################

# Function to generate a ButterDonut network
def generate_butterdonut_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float) -> dict:
    # Validate inputs: For a ButterDonut, n_routers must be a square number and the square root must also be a power of 2
    sqrt_n = int(n_routers ** 0.5)
    log2_sqrt_n = int(math.log2(sqrt_n))
    if sqrt_n * sqrt_n != n_routers or 2 ** log2_sqrt_n != sqrt_n:
        hlp.print_error("ERROR: For a ButterDonut topology, the number of routers must be a perfect square and the square root must be a power of 2.")
        sys.exit(1)
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = [((col + 0.5) * width, (row + 0.5) * height) for row in range(sqrt_n) for col in range(sqrt_n)]
    # Generate topology
    # Horizontal links
    for row in range(sqrt_n):
        for col in range(sqrt_n):
            # Router location
            other_col = (min(col + 2, sqrt_n - 1) if col % 2 == 0 else max(col - 2, 0))
            rid = row * sqrt_n + col
            other_rid = row * sqrt_n + other_col
            link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
            add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
    # Diagonal links
    for stage in range(log2_sqrt_n):
        for col in range(2**stage-1, sqrt_n, 2**(stage+1)):
            for row1 in range(sqrt_n):
                row2 = ((row1 // 2**(stage+1)) * 2**(stage+1)) + ((row1 + 2**stage) % 2**(stage+1))
                rid1 = row1 * sqrt_n + col
                rid2 = row2 * sqrt_n + (col + 1)
                link_len = hlp.manhattan_distance(router_locations[rid1], router_locations[rid2])
                add_bidirectional_link(adj_list, lengths, latencies, rid1, rid2, link_len, link_latency_function(link_len))
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

##########################################################################################
# Kite 
##########################################################################################

# Function to generate a Kite network
def generate_kite_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float, patterns : list[list[tuple[int,int]]]) -> dict:
    # Validate inputs: For a Kite, n_routers must be a square number
    sqrt_n = int(n_routers ** 0.5)
    if sqrt_n * sqrt_n != n_routers:
        hlp.print_error("ERROR: For a Kite topology, the number of routers must be a perfect square.")
        sys.exit(1)
    # Initialize network
    adj_list = [[] for _ in range(n_routers)]
    lengths = [[] for _ in range(n_routers)]
    latencies = [[] for _ in range(n_routers)]
    node_counts = [nodes_per_router for _ in range(n_routers)]
    node_latencies = [[node_link_latency for _ in range(nodes_per_router)] for _ in range(n_routers)]
    router_locations = [((col + 0.5) * width, (row + 0.5) * height) for row in range(sqrt_n) for col in range(sqrt_n)]
    # Auxiliary variables to keep track of radix (limited to 4)
    radix_counts = [0 for _ in range(n_routers)]
    # Generate topology
    is_first = True
    for pattern in patterns:
        # Calculate router location
        for (hor,ver) in pattern:
            for row in range(sqrt_n):
                for col in range(sqrt_n):
                    rid = row * sqrt_n + col
                    row2 = (row + ver)
                    col2 = (col + hor)
                    other_rid = row2 * sqrt_n + col2
                    if (0 <= row2 < sqrt_n) and (0 <= col2 < sqrt_n) and (radix_counts[rid] < 4) and (radix_counts[other_rid] < 4):
                        link_len = hlp.manhattan_distance(router_locations[rid], router_locations[other_rid])
                        add_bidirectional_link(adj_list, lengths, latencies, rid, other_rid, link_len, link_latency_function(link_len))
                        radix_counts[rid] += 1
                        radix_counts[other_rid] += 1
            is_first = False
    # Compose network information and return
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network


##########################################################################################
# WSI Aligned
##########################################################################################

# Function to generate a WSI Aligned network
def generate_wsi_network(n_routers : int, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency : int, width : float, height : float, method : str) -> dict:
    # Validate that the method is valid
    if method not in ["aligned", "interleaved", "rotated", "baseline"]:
        hlp.print_error("ERROR: For a WSI topology, the method must be one of: aligned, interleaved, rotated, baseline.")
        sys.exit(1)
    # Validate the combination of n_routers and method
    if (method in ["baseline", "aligned", "interleaved"] and n_routers != 64) or (method == "rotated" and n_routers != 66):
        hlp.print_error("ERROR: For a WSI topology with method '%s', the number of routers must be %d." % (method, 64 if method in ["aligned", "interleaved"] else 66))
        sys.exit(1)
    # Generate a wafer-scale system using the code for WSI
    design = {
        "integration_level" : "logic_and_interconnect",     # We only analyze this version
        "wafer_utilization" : "maximized",                  # For now, we always maximize utilization
        "method" : ("ours_" + method) if method != "baseline" else method,  # Method to use
        "wafer_diameter" : 300,                             # in mm, for now, we only support 300mm wafers which results in 64 compute reticles for aligned and interleaved, and 66 compute reticles for rotated
    }
    parameters = {
        "reticle_size" : (float(width), float(height)),  # in mm
        "shape_param" : 0.4,
        "node_count" : nodes_per_router,
    }
    system = construct_system(design, parameters)
    # Compose network information and return
    adj_list, lengths, latencies = system.get_adj_list_and_link_latencies(link_latency_function)
    node_counts = system.get_node_counts()
    node_latencies = [([node_link_latency for _ in range(nodes_per_router)] if nc > 0 else None) for nc in node_counts]
    router_locations = system.get_router_locations()
    network = {"adj_list": adj_list, "lengths": lengths, "latencies": latencies, "node_counts": node_counts, "node_latencies": node_latencies, "router_locations": router_locations}
    return network

# Function to generate a network
# - n_routers: Total number of routers in the network
# - nodes_per_router: Number of nodes connected to each router 
# - link_latency: Latency of links between routers
# - node_link_latency: Latency of links between nodes and routers
# - width: Width of one component (macro, chiplet, reticle, etc.)
# - height: Height of one component (macro, chiplet, reticle, etc.)
# RETURNS: A dictionary containing the network information
#   - "adj_list": Adjacency list representing the network topology
#   - "latencies": Latency list corresponding to the adjacency list
#   - "node_counts": List of number of nodes connected to each router
#   - "node_latencies": List of latencies for each node connected to each router
#   - "router_locations": List of (x, y) coordinates for each router
def generate_network(n_routers : int, topology : str, nodes_per_router : int, link_latency_function : Callable[[float], int], node_link_latency_function : int, width : float, height : float) -> dict:
    if topology == "mesh":
        return generate_mesh_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height)
    elif topology == "torus":
        return generate_torus_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height)
    elif topology == "folded_torus":
        return generate_folded_torus_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height)
    elif topology == "flattened_butterfly":
        return generate_flattened_butterfly_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height)
    elif topology == "hexamesh":
        return generate_hexamesh_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height)
    elif topology == "foldedhexatorus":
        return generate_foldedhexatorus_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height)
    elif topology == "hypercube":
        return generate_hypercube_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height)
    elif topology == "double_butterfly":
        return generate_double_butterfly_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height)
    elif topology == "butterdonut":
        return generate_butterdonut_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height)
    elif topology == "kite_small":
        patterns = [[(-1,1),(1,1)],[(1,0),(0,1)]]
        return generate_kite_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height, patterns)
    elif topology == "kite_medium":
        patterns = [[(0,2),(2,0)],[(-1,1),(1,1)],[(1,0),(0,1)]]
        return generate_kite_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height, patterns)
    elif topology == "kite_large":
        patterns = [[(-2,1),(-1,2),(1,2),(2,1)],[(0,2),(2,0)],[(-1,1),(1,1)],[(1,0),(0,1)]]
        return generate_kite_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height, patterns)
    elif topology == "wsi_mesh":
        return generate_wsi_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height, "baseline")
    elif topology == "wsi_aligned":
        return generate_wsi_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height, "aligned")
    elif topology == "wsi_interleaved":
        return generate_wsi_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height, "interleaved")
    elif topology == "wsi_rotated":
        return generate_wsi_network(n_routers, nodes_per_router, link_latency_function, node_link_latency_function, width, height, "rotated")
    else:
        hlp.print_error("ERROR: Unsupported topology %s." % topology)
        sys.exit(1)
        return {}

