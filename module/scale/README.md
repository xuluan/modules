# Scale

## Objective

Apply various scaling methods to seismic trace data. 

User-defined Parameters:

- factor: Apply constant or attribute-based scaling
  - value: constant factor (e.g., 1.3, 5)
- agc: Apply AGC scaling
  - window_size: window length in ms (e.g., 500 ms)
- diverge: Apply divergence scaling
  - a: user-defined exponent (typically 1.0-2.0, default 2.0)
  - v: speed factor / magnification factor (default 1.0)

## Key Features

Supports the following scaling methods:

Method 1: Constant or Attribute-Based Scaling
Option 1 – Constant Scaling: Multiply all data samples by a constant factor (e.g., 1.3, 5).
Option 2 – Trace Attribute Scaling: Use a specific trace attribute (e.g., inline) to scale all samples within each corresponding trace.

Method 2: Automatic Gain Control (AGC)
Apply automatic gain control to normalize amplitude variations.
Define a window length (e.g., 500 ms) to compute local scaling.

Method 3: Divergence Scaling
Scale samples based on their time position using the formula: Sample Value = Sample Value × T^x where:
T = sample time (in seconds)
x = user-defined exponent



