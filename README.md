## Reproducibility Guide

This repository accompanies the paper:

**PAISF: A _Partially Adaptive In-Order Selection Function for Routing in On-Chip, Inter-Chiplet, and Wafer-Scale Networks_**

and contains all code and data required to reproduce the reported results.

### Setup

Before running experiments, build the necessary binaries:

```bash
cd booksim2/src
make
cd ../../
```

---

### Reproducing Experimental Results

To reproduce all results, run:
```bash
python3 main.py -m
```

- Outputs are stored in the `results` directory
- Runtime is approximately **3 weeks on a 64-core machine**
- The script spawns **one thread per core**
- The `results` directory already contains all generated data
- Results are only recomputed if existing files are **deleted or moved**

---

### Generating Plots

To generate all plots, run:
```bash
python3 plots.py
```

- Reads input from the `results` directory
- Stores figures in the `plots` directory
- Pre-generated plots are already included
- Existing plots will be **overwritten**

---

### Verifying Virtual Channel Requirements

To verify our claim that **LASH** and **DFSSSP** require up to **6** and **8 virtual channels**, respectively, on our evaluated topologies, run:
```bash
python3 explore_vc_req.py
python3 reports.py --vc_req
```
