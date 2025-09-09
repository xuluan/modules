# Mute

## Objective

Perform muting correction (zeroing) on seismic data based on attribute values or absolute thresholds.

User-defined Parameters:

- compare_direction: specifies the time point before or after which the value should be muted, indicated by "<" and ">".
- threshold:
  - value: time threshold value in ms.
  - expr: The expression calculates the time threshold value, which can include the following operators: +, -, *, /, sin, cos, tan, (, ), as well as integers, floating-point numbers and attribute names.
- tapering_window_size: the time window size in ms. Positive values increase the correction region, negative values decrease it, and 0 disables smoothing. 

## Key Features

- Absolute value muting correction: sets sample values to zero if they meet specified conditions, e.g.:  time < 2000ms or time > 8000ms
- Attribute-Based muting Correction: sets sample values to zero using attribute-based conditions, e.g.:  time < WB or time > WB ("WB" is an attribute name) 
- Gradient: Applies a linear gradient function to smooth transitions into the muting correction region. 