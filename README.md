# Python Static Timing Analyzer (STA)

A custom static timing analysis tool written in Python that parses structural Verilog, Liberty timing libraries, and SDC clock constraints to evaluate circuit timing, identify critical paths, and calculate setup slack.

The project explores how timing information propagates through digital circuits and how cell delays, fanout capacitance, and setup constraints affect circuit performance.

The analyzer also exports timing-critical nets for integration with a separate **C++ timing-driven VLSI router**.

## Overview

Static Timing Analysis (STA) determines whether signals can propagate through a digital circuit within specified timing constraints without requiring functional simulation.

This project implements a simplified gate-level STA flow that:

1. Parses circuit connectivity from structural Verilog.
2. Extracts cell timing information from Liberty.
3. Reads clock-period constraints from SDC.
4. Constructs net connectivity and calculates capacitive loads.
5. Propagates signal arrival times through combinational logic.
6. Reconstructs register-to-register timing paths.
7. Calculates setup slack and identifies the critical path.
8. Exports critical nets for timing-driven physical routing.

## Features

### Structural Verilog Parsing

Implements a custom parser to extract:

- Standard-cell instances and cell types
- Named pin-to-net connections
- Primary inputs and outputs
- Internal wires and signal connectivity

The parsed circuit representation is used to determine how signals travel between cells.

### Liberty Timing Analysis

Parses Liberty (`.lib`) files to extract:

- Cell and pin definitions
- Pin direction and input capacitance
- Input-to-output timing arcs
- Rising and falling propagation delays
- Clock-to-Q delays
- Setup-time constraints
- Timing lookup tables

Cell delays are calculated using output load capacitance and linear interpolation between characterized capacitance points.

The current implementation uses the first input-transition row of each delay table and selects the larger of the rise and fall delays for a simplified worst-case analysis.

### Arrival-Time Propagation

Constructs a net-level connectivity representation and propagates signal arrival times through combinational gates.

For each combinational output, the analyzer evaluates the available input-to-output timing arcs and retains the maximum arrival time.

The propagation model includes:

- Primary inputs initialized with zero arrival time
- Flip-flop outputs initialized using clock-to-Q delay
- Output load capacitance calculated from connected input pins
- Maximum-delay propagation through combinational logic
- Predecessor tracking for critical-path reconstruction

### Critical-Path Analysis

Traces the worst-arriving signal path backward from each register's setup-constrained data input.

For each path, the analyzer reports:

- Launch and capture flip-flops
- Sequence of cells and nets
- Individual cell delays
- Data arrival time
- Capture-register setup time
- Total timing delay
- Setup slack

Setup slack is calculated using:

**Slack = Clock Period − (Data Arrival Time + Setup Time)**

A path with negative setup slack violates the simplified timing constraint.

### Multi-Endpoint Timing Comparison

Analyzes paths terminating at multiple capture flip-flops and ranks them from largest total delay to smallest.

This makes it possible to identify the most timing-critical register-to-register path in a circuit containing multiple logic branches.

### Timing-Driven Router Integration

Exports the critical path's net names to `critical_nets.txt`.

This output is used by a companion C++ VLSI routing project to identify timing-sensitive connections.

**Companion project:** [Timing-Driven VLSI Router](https://github.com/Adi-Jain9/timing-driven-vlsi-router)

The router currently parses placement, LEF, and Verilog data, builds a routing grid, and imports STA-generated critical nets. Timing-priority path
