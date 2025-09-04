#!/usr/bin/env python3
"""
Module management tool: list modules and their versions, build and install modules, or clean build artifacts.

Usage:
    # List modules
    python module_manager.py --list                      # List all modules
    python module_manager.py -l                          # List all modules (short form)
    python module_manager.py --list --modules attrcalc   # List specific module
    python module_manager.py -l -m attrcalc gendata      # List multiple modules
    
    # Build and install modules
    python module_manager.py --build                     # Build all modules
    python module_manager.py -b                          # Build all modules (short form)  
    python module_manager.py --build --modules attrcalc  # Build specific module
    python module_manager.py -b -m attrcalc gendata      # Build multiple modules
    python module_manager.py -b -m attrcalc --no-confirm # Build without confirmation
    
    # Clean build artifacts
    python module_manager.py --clean                     # Clean all modules
    python module_manager.py -c                          # Clean all modules (short form)
    python module_manager.py --clean --modules attrcalc  # Clean specific module
    python module_manager.py -c -m attrcalc gendata      # Clean multiple modules

Functions Overview:
    Version-related functions:
    - extract_version_from_content(): Extract version using regex
    - fix_json_braces(): Fix malformed JSON by adding missing braces
    - parse_version_from_json_file(): Parse version from JSON file with error handling
    - get_module_json_path(): Construct path to module's JSON file
    - should_process_module(): Determine if module should be processed
    - process_single_module(): Process one module to extract version
    - get_all_module_directories(): Get all module directory paths
    - get_module_versions(): Main logic to extract versions from all/selected modules
    
    Build-related functions:
    - find_build_script(): Find the build script for a module
    - run_build_script(): Execute the build script with version
    - run_ninja_install(): Run ninja install in mybuild directory
    - build_single_module(): Build and install a single module
    - build_modules(): Build and install multiple modules
    
    Clean-related functions:
    - clean_single_module(): Clean build artifacts for a single module
    - clean_modules(): Clean build artifacts for multiple modules
    
    UI and utility functions:
    - create_argument_parser(): Configure command line argument parser
    - get_modules_directory(): Get modules directory path
    - print_scan_header(): Print scan operation header
    - print_build_header(): Print build operation header
    - print_clean_header(): Print clean operation header
    - print_results_table(): Print results in formatted table
    - print_summary(): Print summary statistics
    - handle_no_modules_found(): Handle no modules found case
    - main(): Main entry point
"""

import argparse
import json
import re
import os
import subprocess
import sys
from pathlib import Path


def extract_version_from_content(content):
    """
    Extract version information from JSON content using regex.
    
    Args:
        content: String content of the JSON file
        
    Returns:
        String version or None if not found
    """
    version_match = re.search(r'"version"\s*:\s*"([^"]*)"', content)
    return version_match.group(1) if version_match else None


def fix_json_braces(content):
    """
    Fix malformed JSON by adding missing braces.
    
    Args:
        content: String content that may be missing braces
        
    Returns:
        String content with proper JSON braces
    """
    content = content.strip()
    if not content.startswith('{') and not content.endswith('}'):
        # Missing both braces
        content = '{' + content + '}'
    elif not content.startswith('{') and content.endswith('}'):
        # Missing opening brace
        content = '{' + content
    elif content.startswith('{') and not content.endswith('}'):
        # Missing closing brace
        content = content + '}'
    return content


def parse_version_from_json_file(json_file_path):
    """
    Parse version from a JSON configuration file.
    
    Args:
        json_file_path: Path to the JSON file
        
    Returns:
        String version or error description
    """
    try:
        with open(json_file_path, 'r', encoding='utf-8') as f:
            content = f.read()
            
            # First try regex pattern to extract version directly
            version = extract_version_from_content(content)
            if version:
                return version
            
            # If regex fails, try JSON parsing with brace fixes
            fixed_content = fix_json_braces(content)
            config = json.loads(fixed_content)
            return config.get('version', 'Unknown')
            
    except json.JSONDecodeError:
        # Fallback: try regex one more time on original content
        try:
            with open(json_file_path, 'r', encoding='utf-8') as f:
                content = f.read()
                version = extract_version_from_content(content)
                if version:
                    return version
                else:
                    print(f"Warning: Could not extract version from {json_file_path}")
                    return "Parse Error"
        except Exception as e:
            print(f"Warning: Error in fallback parsing {json_file_path}: {e}")
            return "Error"
    except Exception as e:
        print(f"Warning: Error reading {json_file_path}: {e}")
        return "Error"


def get_module_json_path(module_dir, module_name):
    """
    Construct the path to a module's JSON configuration file.
    
    Args:
        module_dir: Path to the module directory
        module_name: Name of the module
        
    Returns:
        Path object pointing to the JSON file
    """
    return module_dir / "src" / f"{module_name}.json"


def should_process_module(module_name, target_modules):
    """
    Determine if a module should be processed based on target list.
    
    Args:
        module_name: Name of the module
        target_modules: List of target modules or None for all
        
    Returns:
        Boolean indicating whether to process this module
    """
    return target_modules is None or module_name in target_modules


def process_single_module(module_dir, module_name):
    """
    Process a single module directory to extract version information.
    
    Args:
        module_dir: Path to the module directory
        module_name: Name of the module
        
    Returns:
        String version or error description
    """
    json_file = get_module_json_path(module_dir, module_name)
    
    if json_file.exists():
        return parse_version_from_json_file(json_file)
    else:
        print(f"Warning: JSON file not found for module {module_name}")
        return "No JSON file"


def get_all_module_directories(modules_dir):
    """
    Get all module directories from the modules directory.
    
    Args:
        modules_dir: Path to the modules directory
        
    Returns:
        List of tuples (module_dir_path, module_name)
    """
    modules_path = Path(modules_dir)
    
    if not modules_path.exists():
        print(f"Error: Modules directory {modules_dir} does not exist")
        return []
    
    module_dirs = []
    for module_dir in modules_path.iterdir():
        if module_dir.is_dir():
            module_dirs.append((module_dir, module_dir.name))
    
    return module_dirs


def get_module_versions(modules_dir, target_modules=None):
    """
    Traverse module directories and extract version information from JSON files.
    
    Args:
        modules_dir: Path to the modules directory
        target_modules: List of specific module names to process, or None for all modules
        
    Returns:
        Dictionary mapping module names to their versions
    """
    module_versions = {}
    module_dirs = get_all_module_directories(modules_dir)
    
    for module_dir, module_name in module_dirs:
        if should_process_module(module_name, target_modules):
            version = process_single_module(module_dir, module_name)
            module_versions[module_name] = version
    
    return module_versions


def find_build_script(module_dir, module_name):
    """
    Find the build script for a module.
    
    Args:
        module_dir: Path to the module directory
        module_name: Name of the module
        
    Returns:
        Path to the build script or None if not found
    """
    build_script = module_dir / f"build-{module_name}.sh"
    if build_script.exists() and build_script.is_file():
        return build_script
    return None


def run_command(command, cwd, description):
    """
    Run a shell command and handle the output.
    
    Args:
        command: Command to run as a list
        cwd: Working directory for the command
        description: Description of what the command does
        
    Returns:
        Tuple (success: bool, stdout: str, stderr: str)
    """
    print(f"Running: {' '.join(command)}")
    print(f"Working directory: {cwd}")
    
    try:
        result = subprocess.run(
            command,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=600  # 10 minute timeout
        )
        
        if result.stdout:
            print(f"Output:\n{result.stdout}")
        if result.stderr:
            print(f"Error output:\n{result.stderr}")
            
        return result.returncode == 0, result.stdout, result.stderr
        
    except subprocess.TimeoutExpired:
        print(f"Error: Command timed out after 10 minutes")
        return False, "", "Command timed out"
    except Exception as e:
        print(f"Error running command: {e}")
        return False, "", str(e)


def run_build_script(build_script, version, module_dir):
    """
    Execute the build script with the specified version.
    
    Args:
        build_script: Path to the build script
        version: Version string to pass to the build script
        module_dir: Path to the module directory
        
    Returns:
        Boolean indicating success
    """
    print(f"\n🔨 Building module with script: {build_script.name}")
    
    # Make sure the script is executable
    try:
        os.chmod(build_script, 0o755)
    except Exception as e:
        print(f"Warning: Could not make script executable: {e}")
    
    # Run the build script
    success, stdout, stderr = run_command(
        ["bash", str(build_script), version],
        cwd=module_dir,
        description=f"Building module with version {version}"
    )
    
    if success:
        print(f"✅ Build script completed successfully")
    else:
        print(f"❌ Build script failed")
        
    return success


def run_ninja_install(module_dir):
    """
    Run ninja install in the mybuild directory.
    
    Args:
        module_dir: Path to the module directory
        
    Returns:
        Boolean indicating success
    """
    mybuild_dir = module_dir / "mybuild"
    
    if not mybuild_dir.exists():
        print(f"❌ mybuild directory not found: {mybuild_dir}")
        return False
    
    print(f"\n⚡ Installing module using ninja")
    
    success, stdout, stderr = run_command(
        ["ninja", "install"],
        cwd=mybuild_dir,
        description="Installing module"
    )
    
    if success:
        print(f"✅ Installation completed successfully")
    else:
        print(f"❌ Installation failed")
        
    return success


def build_single_module(module_dir, module_name, version):
    """
    Build and install a single module.
    
    Args:
        module_dir: Path to the module directory
        module_name: Name of the module
        version: Version to build
        
    Returns:
        Boolean indicating overall success
    """
    print(f"\n{'='*60}")
    print(f"🚀 Processing module: {module_name} (version: {version})")
    print(f"📁 Module directory: {module_dir}")
    
    # Find build script
    build_script = find_build_script(module_dir, module_name)
    if not build_script:
        print(f"❌ Build script not found: build-{module_name}.sh")
        return False
    
    # Run build script
    build_success = run_build_script(build_script, version, module_dir)
    if not build_success:
        print(f"❌ Build failed for module {module_name}")
        return False
    
    # Run ninja install
    install_success = run_ninja_install(module_dir)
    if not install_success:
        print(f"❌ Installation failed for module {module_name}")
        return False
    
    print(f"🎉 Module {module_name} built and installed successfully!")
    return True


def build_modules(modules_dir, target_modules=None):
    """
    Build and install multiple modules.
    
    Args:
        modules_dir: Path to the modules directory
        target_modules: List of specific module names to build, or None for all
        
    Returns:
        Dictionary mapping module names to build success status
    """
    # First get the versions for all modules we want to build
    module_versions = get_module_versions(modules_dir, target_modules)
    
    if not module_versions:
        return {}
    
    build_results = {}
    successful_builds = 0
    failed_builds = 0
    
    print(f"\n🏗️  Starting build process for {len(module_versions)} module(s)")
    
    for module_name, version in sorted(module_versions.items()):
        if version in ["Parse Error", "Error", "No JSON file"]:
            print(f"\n❌ Skipping {module_name}: Cannot determine version ({version})")
            build_results[module_name] = False
            failed_builds += 1
            continue
        
        module_dirs = get_all_module_directories(modules_dir)
        module_dir = None
        for dir_path, name in module_dirs:
            if name == module_name:
                module_dir = dir_path
                break
        
        if not module_dir:
            print(f"\n❌ Module directory not found for {module_name}")
            build_results[module_name] = False
            failed_builds += 1
            continue
        
        success = build_single_module(module_dir, module_name, version)
        build_results[module_name] = success
        
        if success:
            successful_builds += 1
        else:
            failed_builds += 1
    
    # Print final summary
    print(f"\n{'='*60}")
    print(f"📊 BUILD SUMMARY")
    print(f"{'='*60}")
    print(f"✅ Successful builds: {successful_builds}")
    print(f"❌ Failed builds: {failed_builds}")
    print(f"📋 Total modules processed: {len(build_results)}")
    
    if successful_builds > 0:
        print(f"\n🎉 Successfully built modules:")
        for module_name, success in sorted(build_results.items()):
            if success:
                print(f"  ✅ {module_name}")
    
    if failed_builds > 0:
        print(f"\n💥 Failed builds:")
        for module_name, success in sorted(build_results.items()):
            if not success:
                print(f"  ❌ {module_name}")
    
    return build_results


def clean_single_module(module_dir, module_name):
    """
    Clean build artifacts for a single module.
    
    Args:
        module_dir: Path to the module directory
        module_name: Name of the module
        
    Returns:
        Boolean indicating success
    """
    print(f"\n{'='*60}")
    print(f"🧹 Cleaning module: {module_name}")
    print(f"📁 Module directory: {module_dir}")
    
    success = True
    items_cleaned = 0
    
    # Clean mybuild directory
    mybuild_dir = module_dir / "mybuild"
    if mybuild_dir.exists():
        try:
            print(f"🗑️  Removing mybuild directory: {mybuild_dir}")
            import shutil
            shutil.rmtree(mybuild_dir)
            print(f"✅ Successfully removed mybuild directory")
            items_cleaned += 1
        except Exception as e:
            print(f"❌ Failed to remove mybuild directory: {e}")
            success = False
    else:
        print(f"ℹ️  mybuild directory not found (already clean)")
    
    # Clean compile_commands.json file
    compile_commands_file = module_dir / "compile_commands.json"
    if compile_commands_file.exists():
        try:
            print(f"🗑️  Removing compile_commands.json file: {compile_commands_file}")
            compile_commands_file.unlink()
            print(f"✅ Successfully removed compile_commands.json")
            items_cleaned += 1
        except Exception as e:
            print(f"❌ Failed to remove compile_commands.json: {e}")
            success = False
    else:
        print(f"ℹ️  compile_commands.json not found (already clean)")
    
    if success:
        if items_cleaned > 0:
            print(f"🎉 Module {module_name} cleaned successfully! ({items_cleaned} items removed)")
        else:
            print(f"✨ Module {module_name} was already clean")
    else:
        print(f"⚠️  Module {module_name} cleaning completed with errors")
    
    return success


def clean_modules(modules_dir, target_modules=None):
    """
    Clean build artifacts for multiple modules.
    
    Args:
        modules_dir: Path to the modules directory
        target_modules: List of specific module names to clean, or None for all
        
    Returns:
        Dictionary mapping module names to clean success status
    """
    module_dirs = get_all_module_directories(modules_dir)
    
    if not module_dirs:
        return {}
    
    # Filter modules based on target_modules
    modules_to_clean = []
    for module_dir, module_name in module_dirs:
        if should_process_module(module_name, target_modules):
            modules_to_clean.append((module_dir, module_name))
    
    if not modules_to_clean:
        return {}
    
    clean_results = {}
    successful_cleans = 0
    failed_cleans = 0
    
    print(f"\n🧹 Starting clean process for {len(modules_to_clean)} module(s)")
    
    for module_dir, module_name in sorted(modules_to_clean, key=lambda x: x[1]):
        success = clean_single_module(module_dir, module_name)
        clean_results[module_name] = success
        
        if success:
            successful_cleans += 1
        else:
            failed_cleans += 1
    
    # Print final summary
    print(f"\n{'='*60}")
    print(f"📊 CLEAN SUMMARY")
    print(f"{'='*60}")
    print(f"✅ Successful cleans: {successful_cleans}")
    print(f"❌ Failed cleans: {failed_cleans}")
    print(f"📋 Total modules processed: {len(clean_results)}")
    
    if successful_cleans > 0:
        print(f"\n✨ Successfully cleaned modules:")
        for module_name, success in sorted(clean_results.items()):
            if success:
                print(f"  ✅ {module_name}")
    
    if failed_cleans > 0:
        print(f"\n💥 Clean failures:")
        for module_name, success in sorted(clean_results.items()):
            if not success:
                print(f"  ❌ {module_name}")
    
    return clean_results


def create_argument_parser():
    """
    Create and configure the command line argument parser.
    
    Returns:
        ArgumentParser configured for the script
    """
    parser = argparse.ArgumentParser(
        description="Manage modules: list versions, build and install modules, or clean build artifacts.",
        epilog="Examples:\n"
               "  %(prog)s --list                           # List all modules\n"
               "  %(prog)s --list --modules attrcalc        # List specific module\n"
               "  %(prog)s --list -m attrcalc gendata       # List multiple modules\n"
               "  %(prog)s --build                          # Build all modules\n"
               "  %(prog)s --build --modules attrcalc       # Build specific module\n"
               "  %(prog)s --build -m attrcalc gendata      # Build multiple modules\n"
               "  %(prog)s --clean                          # Clean all modules\n"
               "  %(prog)s --clean --modules attrcalc       # Clean specific module\n"
               "  %(prog)s --clean -m attrcalc gendata      # Clean multiple modules\n"
               "  %(prog)s -l -m attrcalc                   # Short form: list specific module\n"
               "  %(prog)s -b -m attrcalc gendata           # Short form: build multiple modules\n"
               "  %(prog)s -c -m attrcalc gendata           # Short form: clean multiple modules",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    # Action group - exactly one required
    action_group = parser.add_mutually_exclusive_group(required=True)
    action_group.add_argument(
        '--list', '-l',
        action='store_true',
        help='List modules and their versions'
    )
    action_group.add_argument(
        '--build', '-b',
        action='store_true',
        help='Build and install modules'
    )
    action_group.add_argument(
        '--clean', '-c',
        action='store_true',
        help='Clean build artifacts (removes mybuild directories and compile_commands.json files)'
    )
    
    # Module selection
    parser.add_argument(
        '--modules', '-m',
        nargs='+',
        metavar='MODULE',
        help='Specific module names to process. If not specified, all modules will be processed.'
    )
    
    # Additional options
    parser.add_argument(
        '--no-confirm',
        action='store_true',
        help='Skip confirmation prompt for build operations (use with caution)'
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )
    
    return parser


def get_modules_directory():
    """
    Get the modules directory path relative to the script location.
    
    Returns:
        Path object pointing to the modules directory
    """
    script_dir = Path(__file__).parent
    return script_dir.parent / "module"


def print_scan_header(target_modules, modules_dir):
    """
    Print the header information for the scan operation.
    
    Args:
        target_modules: List of target modules or None for all
        modules_dir: Path to the modules directory
    """
    print("Module Version Scanner")
    print("=" * 40)
    if target_modules:
        print(f"Scanning specific modules: {', '.join(target_modules)}")
    else:
        print("Scanning all modules")
    print(f"Module directory: {modules_dir}")
    print()


def print_build_header(target_modules, modules_dir):
    """
    Print the header information for the build operation.
    
    Args:
        target_modules: List of target modules or None for all
        modules_dir: Path to the modules directory
    """
    print("🏗️  Module Build & Install System")
    print("=" * 60)
    if target_modules:
        print(f"🎯 Building specific modules: {', '.join(target_modules)}")
    else:
        print("🌟 Building all modules")
    print(f"📁 Module directory: {modules_dir}")
    print(f"⚠️  This will run build scripts and install modules")
    print()


def print_clean_header(target_modules, modules_dir):
    """
    Print the header information for the clean operation.
    
    Args:
        target_modules: List of target modules or None for all
        modules_dir: Path to the modules directory
    """
    print("🧹 Module Clean System")
    print("=" * 60)
    if target_modules:
        print(f"🎯 Cleaning specific modules: {', '.join(target_modules)}")
    else:
        print("🌟 Cleaning all modules")
    print(f"📁 Module directory: {modules_dir}")
    print(f"⚠️  This will remove mybuild directories and compile_commands.json files")
    print()


def print_results_table(module_versions):
    """
    Print the results in a formatted table.
    
    Args:
        module_versions: Dictionary mapping module names to versions
    """
    print(f"{'Module Name':<20} {'Version':<15}")
    print("-" * 40)
    
    for module_name, version in sorted(module_versions.items()):
        print(f"{module_name:<20} {version:<15}")


def print_summary(module_versions, target_modules):
    """
    Print the summary statistics.
    
    Args:
        module_versions: Dictionary mapping module names to versions
        target_modules: List of target modules or None for all
    """
    print()
    if target_modules:
        print(f"Requested modules processed: {len(module_versions)}")
    else:
        print(f"Total modules found: {len(module_versions)}")


def handle_no_modules_found(target_modules, modules_dir):
    """
    Handle the case when no modules were found.
    
    Args:
        target_modules: List of target modules or None for all
        modules_dir: Path to the modules directory
    """
    if target_modules:
        print("No matching modules found.")
        # Check if specified modules exist
        all_modules = get_module_versions(modules_dir, None)
        available_modules = set(all_modules.keys())
        requested_modules = set(target_modules)
        missing_modules = requested_modules - available_modules
        if missing_modules:
            print(f"Modules not found: {', '.join(sorted(missing_modules))}")
            print(f"Available modules: {', '.join(sorted(available_modules))}")
    else:
        print("No modules found.")


def main():
    """Main function to execute the script."""
    # Parse command line arguments
    parser = create_argument_parser()
    args = parser.parse_args()
    
    # Get paths and target modules
    modules_dir = get_modules_directory()
    target_modules = args.modules if args.modules else None
    
    # Determine the action based on flags
    if args.list:
        # List modules and versions
        print_scan_header(target_modules, modules_dir)
        
        # Get module versions
        module_versions = get_module_versions(modules_dir, target_modules)
        
        # Handle results
        if not module_versions:
            handle_no_modules_found(target_modules, modules_dir)
            return
        
        # Print results
        print_results_table(module_versions)
        print_summary(module_versions, target_modules)
        
    elif args.build:
        # Build and install modules
        print_build_header(target_modules, modules_dir)
        
        # Skip confirmation if --no-confirm flag is set
        if not args.no_confirm:
            # Confirm with user before proceeding with build
            if target_modules:
                confirmation_msg = f"Are you sure you want to build and install {len(target_modules)} module(s): {', '.join(target_modules)}?"
            else:
                # Get count of all modules
                all_modules = get_module_versions(modules_dir, None)
                module_count = len(all_modules)
                confirmation_msg = f"Are you sure you want to build and install ALL {module_count} modules?"
            
            print(f"⚠️  {confirmation_msg}")
            print("This will:")
            print("  1. Run build-<module>.sh <version> for each module")
            print("  2. Execute 'ninja install' in each module's mybuild directory")
            print("  3. This may take a significant amount of time")
            print()
            
            try:
                response = input("Continue? [y/N]: ").strip().lower()
            except KeyboardInterrupt:
                print("\n\n🚫 Build cancelled by user.")
                return
            
            if response not in ['y', 'yes']:
                print("🚫 Build cancelled by user.")
                return
        else:
            print("🚀 Proceeding without confirmation (--no-confirm flag set)")
        
        # Proceed with build
        build_results = build_modules(modules_dir, target_modules)
        
        if not build_results:
            handle_no_modules_found(target_modules, modules_dir)
            return
        
        # Exit with appropriate code
        failed_count = sum(1 for success in build_results.values() if not success)
        if failed_count > 0:
            print(f"\n💥 {failed_count} module(s) failed to build. Exiting with error code.")
            sys.exit(1)
        else:
            print(f"\n🎉 All modules built successfully!")
            sys.exit(0)
    
    elif args.clean:
        # Clean build artifacts
        print_clean_header(target_modules, modules_dir)
        
        # Skip confirmation if --no-confirm flag is set
        if not args.no_confirm:
            # Confirm with user before proceeding with clean
            if target_modules:
                confirmation_msg = f"Are you sure you want to clean {len(target_modules)} module(s): {', '.join(target_modules)}?"
            else:
                # Get count of all modules
                all_modules = get_module_versions(modules_dir, None)
                module_count = len(all_modules)
                confirmation_msg = f"Are you sure you want to clean ALL {module_count} modules?"
            
            print(f"⚠️  {confirmation_msg}")
            print("This will:")
            print("  1. Remove 'mybuild' directories from each module")
            print("  2. Remove 'compile_commands.json' files from each module")
            print("  3. This will permanently delete build artifacts")
            print()
            
            try:
                response = input("Continue? [y/N]: ").strip().lower()
            except KeyboardInterrupt:
                print("\n\n🚫 Clean cancelled by user.")
                return
            
            if response not in ['y', 'yes']:
                print("🚫 Clean cancelled by user.")
                return
        else:
            print("🚀 Proceeding without confirmation (--no-confirm flag set)")
        
        # Proceed with clean
        clean_results = clean_modules(modules_dir, target_modules)
        
        if not clean_results:
            handle_no_modules_found(target_modules, modules_dir)
            return
        
        # Exit with appropriate code
        failed_count = sum(1 for success in clean_results.values() if not success)
        if failed_count > 0:
            print(f"\n💥 {failed_count} module(s) failed to clean. Exiting with error code.")
            sys.exit(1)
        else:
            print(f"\n🎉 All modules cleaned successfully!")
            sys.exit(0)


if __name__ == "__main__":
    main()