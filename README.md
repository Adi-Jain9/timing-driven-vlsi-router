# Timing-Driven VLSI Router

A C++ grid-based routing tool that uses **congestion-aware Dijkstra's algorithm** to connect standard cells while prioritizing timing-critical nets identified by a Python static timing analyzer.

The project explores how graph algorithms, routing order, and congestion penalties can be used to optimize physical connections in VLSI design.

## Features

- **Timing-Driven Routing:** Imports critical-net information from a Python static timing analyzer and routes critical nets before noncritical nets.
- **Congestion-Aware Dijkstra:** Uses a priority queue and a congestion-weighted cost function to find low-cost routing paths.
- **Manhattan-Distance Ordering:** Prioritizes shorter connections within the same timing-priority group.
- **Multi-Terminal Routing:** Maintains routing trees so multiple destinations on the same net can connect to previously routed wire segments.
- **Obstacle Avoidance:** Prevents routes from crossing standard-cell footprints or wires belonging to other nets.
- **LEF and Verilog Parsing:** Extracts cell geometry, pin coordinates, and electrical connections.
- **ASCII Visualization:** Displays cell placement and routed paths on a two-dimensional grid.

## How It Works

### 1. Parse Design Inputs

The router reads four input files:

| File | Purpose |
|---|---|
| `placement.txt` | Defines cell instances and their coordinates |
| `cell_geometry.lef` | Defines cell dimensions and pin geometries |
| `connections.v` | Defines electrical connections between cell pins |
| `critical_nets.txt` | Identifies critical nets from the Python timing analyzer |

The tool combines these inputs to determine where cells are located, which pins need to connect, and which nets should receive routing priority.

### 2. Construct the Routing Grid

The chip is represented using a two-dimensional grid.

- Standard-cell footprints are marked as blocked.
- Pin access points are calculated immediately outside cell boundaries.
- Each grid position tracks whether it is occupied by a wire and which electrical net owns it.

The router uses four-directional movement: up, down, left, and right.

### 3. Prioritize Routing Tasks

Routing tasks are ordered using two criteria:

1. **Timing criticality:** Critical nets are prioritized over noncritical nets.
2. **Manhattan distance:** Within the same criticality group, shorter connections are prioritized.

Manhattan distance is calculated as:

$$
D = |x_2-x_1| + |y_2-y_1|
$$

This provides a geometric estimate of connection length before routing.

### 4. Run Congestion-Aware Dijkstra

The router uses Dijkstra's shortest-path algorithm with a min-priority queue.

Each grid movement has a cost:

$$
C_{\text{step}} = 1 + \lambda N_{\text{adjacent}}
$$

Where:

- \(1\) is the base movement cost.
- \(\lambda\) is the congestion weight.
- \(N_{\text{adjacent}}\) is the number of neighboring positions occupied by other nets.

The current implementation uses a congestion weight of 2.

The congestion penalty discourages paths from traveling close to existing wires, while blocked positions and wires belonging to other nets are treated as unavailable.

Dijkstra minimizes total accumulated routing cost, which may differ from the physically shortest path.

### 5. Build Multi-Terminal Routing Trees

For nets with multiple destinations, the router maintains a set of connected grid positions.

After routing the first destination, subsequent destinations can connect to any point on the existing routing tree rather than always returning to the original driver.

This allows the router to reuse previously established connections and reduce redundant routing.

### 6. Generate Routing Results

After routing, the program reports:

- Successfully routed connections
- Failed connections
- Individual route lengths
- Total reported routing length in grid steps
- ASCII visualization of the chip and its routed wires

## Results

The router was evaluated on a sample circuit containing 15 standard-cell instances and 16 internal driver-to-destination routing tasks.

| Metric | Result |
|---|---|
| Standard-cell instances | 15 |
| Routing tasks | 16 |
| Successfully routed tasks | 15 |
| Routing completion | 93.75% |
| Critical routing tasks | 10 |
| Successfully routed critical tasks | 10/10 |
| Reported total route length | 166 grid steps |
| Failed connection | `n6` |

All 10 timing-critical routing tasks completed successfully.

The remaining noncritical connection, `n6`, could not be completed under the existing single-layer routing constraints.

### Routing Order Optimization

Two routing-order approaches were evaluated:

| Metric | Critical-First Ordering | Critical-First + Manhattan Ordering |
|---|---|---|
| Successful routes | 15/16 | 15/16 |
| Critical routes completed | 10/10 | 10/10 |
| Reported total route length | 232 steps | 166 steps |

Adding Manhattan-distance ordering reduced the reported total route length by approximately **28.4%** while maintaining the same routing completion rate.

These totals represent the sum of successful path lengths returned by the routing algorithm, rather than independently verified unique physical wire length. Since the failed nets differ between runs, the reduction should not be interpreted as a like-for-like improvement across an identical set of completed connections.

## Building and Running

### Requirements

- C++11-compatible compiler
- Design input files in the expected format
- Critical-net output from the companion Python timing analyzer

### Compile

```bash
g++ -std=c++11 router.cpp -o router
```

### Run

```bash
./router
```

The program expects `placement.txt`, `cell_geometry.lef`, and `connections.v` in its working directory.

It currently reads timing-critical nets from:

```text
../timing_analyzer/critical_nets.txt
```

### Example Output

```text
Dijkstra Routing Results:
CRITICAL: n8 -> INV7.A | Length: 1
CRITICAL: n8 -> NAND4.B | Length: 13
CRITICAL: q1 -> INV1.A | Length: 2
CRITICAL: n1 -> INV3.A | Length: 3
CRITICAL: n1 -> INV2.A | Length: 6
...
NORMAL: n7 -> NAND3.B | Length: 14
FAILED: n6 -> NAND4.A
...

Routing Summary:
Successful tasks: 15
Failed tasks: 1
Total tasks: 16
Total added wire length: 166 grid steps
```

## Current Limitations

This project is an educational physical-routing prototype rather than a complete production VLSI router.

Current limitations include:

- Single-layer grid routing without vias or multilayer metal support
- No rip-up-and-reroute when a connection fails
- Simplified LEF and Verilog parsers designed for the sample input files
- Simplified cell-pin access geometry
- No post-routing parasitic extraction or timing analysis
- No comprehensive design-rule checking or final connectivity verification

Critical-net prioritization is based on the static timing analyzer's output. The router does not currently recalculate timing after placing wires.

## Future Improvements

- Add rip-up-and-reroute to resolve routing conflicts.
- Support multiple metal layers and vias.
- Add final connectivity and routing-rule verification.
- Measure unique routed wire length and congestion statistics.
- Incorporate estimated wire delay into routing costs.
- Re-evaluate timing after routing and use updated slack to guide optimization.

## Project Motivation

This project was developed to explore the relationship between graph algorithms and physical design automation.

Building a static timing analyzer in Python introduced the importance of critical paths, setup slack, and timing constraints. This router extends that work by using critical-net information to influence physical routing decisions.

The broader goal is to understand how software algorithms can improve hardware-design efficiency, particularly through timing-aware optimization, congestion management, and automated physical implementation.
