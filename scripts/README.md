# GeoDelity Development Scripts

This directory contains development automation scripts for the GeoDelity seismic data processing framework. These tools streamline module building, testing, and deployment workflows.

## 🛠️ Scripts Overview

| Script | Purpose | Main Functions |
|--------|---------|----------------|
| `gd_builder.py` | Module Build Manager | List versions, build & install modules |
| `gd_tester.py` | Test Pipeline Manager | Copy test files, run tests, generate reports |
| `test_runner.py` | Test Execution Engine | Low-level test execution and reporting |
| `test_copy_utils.py` | Test File Utilities | Module test file discovery and copying |

---

## 🏗️ gd_builder.py - Module Build Manager

**Purpose**: Manage module versions, building, and installation across the GeoDelity framework.

### Features
- ✅ **Version Discovery**: Scan module JSON configs to extract version information
- 🔨 **Automated Building**: Execute build scripts with proper version handling
- ⚡ **Parallel Installation**: Run ninja install for compiled modules
- 📊 **Build Reporting**: Comprehensive success/failure tracking
- 🛡️ **Error Recovery**: Robust JSON parsing with fallback mechanisms

### Usage Examples

```bash
# List all module versions
python scripts/gd_builder.py --list

# List specific modules
python scripts/gd_builder.py --list -m attrcalc testexpect

# Build all modules (with confirmation)
python scripts/gd_builder.py --build

# Build specific modules without confirmation
python scripts/gd_builder.py --build -m attrcalc gendata --no-confirm

# Verbose output for debugging
python scripts/gd_builder.py --list --verbose
```

### Build Process
1. **Version Detection**: Parse `module/*/src/*.json` files for version info
2. **Script Execution**: Run `build-<module>.sh <version>` in module directory
3. **Compilation**: Execute CMake + Ninja build process
4. **Installation**: Run `ninja install` in `mybuild/` directory
5. **Reporting**: Display success/failure summary with details

### Supported Modules
- `attrcalc` (v1.1.0) - Mathematical expression calculator
- `gendata` (v1.1.0) - Synthetic seismic data generator
- `testexpect` (v1.0.0) - Data validation framework
- `input` (v1.0.0) - Data input/loading module
- `output` (v1.3.0) - Data output/export module
- `attrlist` (v1.0.0) - Attribute listing utilities
- `demoattr` (v1.0.0) - Demonstration attributes
- `testgendata` (v1.0.0) - Test data generation

---

## 🧪 gd_tester.py - Test Pipeline Manager

**Purpose**: Comprehensive test management with automated copying and execution workflows.

### Features
- 📂 **Smart Test Discovery**: Find and copy test files from module directories
- 🔄 **Pipeline Automation**: Execute copy → test workflow automatically
- 🧹 **Environment Management**: Clean and reset test/grun directories
- 📋 **Pattern Matching**: Support wildcards for selective test copying
- 📊 **Comprehensive Reporting**: Detailed test results with execution logs
- 🔧 **Environment Control**: Manage GEODELITY_DIR, debug flags, job retention

### Usage Examples

```bash
# Run complete test pipeline (copy + execute)
python scripts/gd_tester.py

# Test specific modules
python scripts/gd_tester.py -m attrcalc testexpect

# Pattern-based test selection (use quotes!)
python scripts/gd_tester.py -m 'testexpect/*attr*'

# Clean directories first, then run tests
python scripts/gd_tester.py --clean

# Dry-run to preview operations
python scripts/gd_tester.py --dry-run --verbose

# Debug mode with job retention
python scripts/gd_tester.py --debug --keepjob --verbose

# Specify GEODELITY_DIR explicitly
python scripts/gd_tester.py --geo /path/to/geodelity
```

### Execution Pipeline
1. **Directory Setup**: Clean and prepare `tests/` and `grun/` directories
2. **Test Discovery**: Scan `module/*/test/*.job` files for test definitions
3. **File Copying**: Copy test files to central `tests/` directory with prefixed names
4. **Environment Setup**: Configure GEODELITY_DIR, GRUN_DIR, debug settings
5. **Test Execution**: Run each test through `grun.sh` with timeout control
6. **Result Analysis**: Parse test outcomes and generate comprehensive reports

### Test File Naming Convention
- **Source**: `module/attrcalc/test/basic_test.job`
- **Destination**: `tests/attrcalc__basic_test_1.1.0.job`
- **Format**: `{module}__{original_name}`

### Environment Variables Managed
- `GEODELITY_DIR` - Main framework directory
- `GRUN_DIR` - Test execution workspace  
- `GDLOGGING_LEVEL` - Debug logging control (`--debug`)
- `KEEPJOBFILES` - Preserve job artifacts (`--keepjob`)

---

## 🔧 Core Libraries

### test_runner.py - Test Execution Engine
**Object-Oriented Architecture:**
- `TestRunner` - Main test coordinator
- `SingleTestRunner` - Individual test execution
- `TestEnvironment` - Environment setup and validation
- `JobConfigParser` - Parse job configuration from test files
- `TestOutputFormatter` - Format test results and summaries

**Key Features:**
- Job configuration parsing from `# {pass: yes, timeout: 300}` comments
- Subprocess management with timeout control
- Environment variable setup and validation
- Comprehensive stdout/stderr capture and formatting

### test_copy_utils.py - Test File Management
**Object-Oriented Architecture:**
- `TestCopier` - Main orchestrator for copy operations
- `ModuleManager` - Module discovery and directory management
- `TestFileManager` - Test file discovery and naming
- `TestFileCopier` - File copying with proper naming conventions
- `ModulePatternParser` - Parse module specifications with wildcards

**Key Features:**
- Shell expansion protection for wildcard patterns
- Module-based test file organization
- Pattern matching with `fnmatch` support
- Dry-run capabilities for safe preview

---

## 🚀 Development Workflow

### Typical Development Cycle

1. **Build Modules**:
   ```bash
   # Build modified modules
   python scripts/gd_builder.py --build -m attrcalc testexpect
   ```

2. **Run Tests**:
   ```bash
   # Test the built modules
   python scripts/gd_tester.py -m attrcalc testexpect --verbose
   ```

3. **Debug Issues**:
   ```bash
   # Run with full debugging
   python scripts/gd_tester.py -m attrcalc --debug --keepjob --verbose
   ```

4. **CI/CD Integration**:
   ```bash
   # Automated build and test
   python scripts/gd_builder.py --build --no-confirm
   python scripts/gd_tester.py --verbose
   ```

### Best Practices

#### For gd_builder.py:
- Use `--no-confirm` in automated environments
- Check build logs for compilation errors
- Verify module installation before testing
- Use `--verbose` for troubleshooting build issues

#### For gd_tester.py:
- Always quote wildcard patterns: `'module/*pattern*'`
- Use `--dry-run` to preview operations before execution
- Use `--clean` to ensure fresh test environment
- Set `GEODELITY_DIR` environment variable or use `--geo`
- Use `--debug --keepjob` for detailed troubleshooting

---

## 📋 Requirements

### System Dependencies
- Python 3.7+
- CMake 3.15+
- Ninja build system
- GCC/Clang compiler
- Apache Arrow (installed at `/usr/local/arrow`)

### Python Dependencies
- `pathlib` - Path manipulation
- `subprocess` - Process execution
- `argparse` - Command-line parsing
- `json` - Configuration parsing
- `shutil` - File operations

### Environment Setup
- `GEODELITY_DIR` - Path to GeoDelity framework installation
- CMake toolchain properly configured
- Module build scripts executable (`chmod +x build-*.sh`)

---

## 🔍 Troubleshooting

### Common Issues

#### Build Failures
```bash
# Check module versions
python scripts/gd_builder.py --list --verbose

# Verify build script exists
ls module/attrcalc/build-attrcalc.sh

# Check dependencies
ninja --version
cmake --version
```

#### Test Failures
```bash
# Verify GEODELITY_DIR
echo $GEODELITY_DIR

# Check test file discovery
python scripts/gd_tester.py --dry-run --verbose

# Debug specific test
python scripts/gd_tester.py -m attrcalc --debug --keepjob --verbose
```

#### Permission Issues
```bash
# Make build scripts executable
chmod +x module/*/build-*.sh

# Check directory permissions
ls -la tests/ grun/
```

### Debug Information
Both tools support verbose output modes that provide detailed execution information:
- Module discovery processes
- File operations and paths
- Environment variable settings
- Command execution with full output
- Error messages with context

---

## 📝 Configuration

### Module Configuration
Each module requires:
- `src/<module>.json` - Version and configuration
- `build-<module>.sh` - Build script
- `test/*.job` - Test definitions (optional)

### Test Configuration
Tests support embedded configuration:
```yaml
# {pass: yes, timeout: 300}
- testgendata:
    version: '1.0.0'
    # ... test definition
```

**Configuration Options:**
- `pass: yes/no` - Expected test outcome
- `timeout: seconds` - Maximum execution time
- Case-insensitive parsing with fallback defaults

---

This documentation provides comprehensive guidance for using the GeoDelity development tools efficiently and effectively.