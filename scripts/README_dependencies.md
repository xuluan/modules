# Python Dependencies for Module Scripts

## Installation

To install the required Python dependencies for the module build and test scripts:

```bash
# Install dependencies
pip install -r requirements.txt
```

## Required Dependencies

- **PyYAML (6.0.2)**: YAML parsing for job file configuration and module information extraction

## Script Dependencies Overview

| Script | Purpose | Key Dependencies |
|--------|---------|------------------|
| `gd_builder.py` | Build management tool | Standard library only |
| `gd_tester.py` | Test management tool | PyYAML |
| `test_runner.py` | Test execution engine | PyYAML |
| `test_copy_utils.py` | File copy utilities | Standard library only |

## Notes

- All scripts use Python standard library modules (json, os, pathlib, etc.) which don't require separate installation
- PyYAML is required for parsing YAML job files and extracting module version information
- The scripts are compatible with Python 3.7+