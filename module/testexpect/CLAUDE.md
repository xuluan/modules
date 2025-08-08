# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Development Commands

### Building the Project
- **Build**: Run `./build-testexpect.sh <version>` to build the project
  - Creates a `mybuild` directory with Ninja build files
  - Requires environment variables from `${GEODELITY_DIR}/etc/env.sh`
  - Uses Ninja build system with CMake
  - Generates `compile_commands.json` for IDE integration

### Environment Dependencies
The build requires several environment variables to be set (handled by the build script):
- `GEODELITY_DIR`: Main project directory
- `MODULECONFIG_DIR`, `GEODATAFLOW_DIR`, `VDSSTORE_DIR`, `GDLOGGER_DIR`: Library directories
- `ARROW_DIR`: Apache Arrow installation path

## Architecture Overview

### Core Components
This is a C++17 data validation/testing module for seismic data processing within the Geodelity framework.

**Main Files:**
- `testexpect.h/cpp`: Core module implementing data validation and expectation checking
- `dynamic_value.h`: Type-safe variant container for configuration data
- `yaml_parser.h`: YAML configuration parser using yaml-cpp
- `testexpect.json`: Configuration schema defining data validation parameters

### Key Architecture Patterns

**Configuration-Driven Validation:**
- Uses YAML configuration files parsed into `DynamicValue` objects
- Defines primary/secondary keys, trace data, and attribute validation rules
- Implements multiple validation patterns (`SAME`, `ATTR_PLUS_MUL`)

**Data Processing Flow:**
1. `testexpect_init()`: Parse configuration, validate against GeoDataFlow framework
2. `testexpect_process()`: Load expected data files, validate against actual data
3. Integration with GeoDataFlow framework for data access and job management

**External Dependencies:**
- GeoDataFlow: Main data processing framework
- ArrowStore: Data storage and format handling
- ModuleConfig: Module configuration management
- GdLogger: Logging infrastructure
- libfort: Table formatting
- yaml-cpp: YAML parsing

### Data Validation System
- Supports multiple data formats (int8/16/32/64, float, double)
- Implements pattern-based validation (e.g., `INLINE+CROSSLINE*2.7`)
- Loads expected data from `.DAT` files for comparison
- Validates data dimensions, types, and values against expected patterns

### Error Handling
- Comprehensive exception handling with descriptive error messages
- Automatic cleanup of resources on errors
- Integration with framework-level job abortion mechanisms