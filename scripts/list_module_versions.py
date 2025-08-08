#!/usr/bin/env python3
"""
Script to list all modules and their versions from JSON configuration files.

Usage:
    python list_module_versions.py                    # List all modules
    python list_module_versions.py attrcalc           # List specific module
    python list_module_versions.py attrcalc gendata   # List multiple modules

Functions Overview:
    - extract_version_from_content(): Extract version using regex
    - fix_json_braces(): Fix malformed JSON by adding missing braces
    - parse_version_from_json_file(): Parse version from JSON file with error handling
    - get_module_json_path(): Construct path to module's JSON file
    - should_process_module(): Determine if module should be processed
    - process_single_module(): Process one module to extract version
    - get_all_module_directories(): Get all module directory paths
    - get_module_versions(): Main logic to extract versions from all/selected modules
    - create_argument_parser(): Configure command line argument parser
    - get_modules_directory(): Get modules directory path
    - print_scan_header(): Print scan operation header
    - print_results_table(): Print results in formatted table
    - print_summary(): Print summary statistics
    - handle_no_modules_found(): Handle no modules found case
    - main(): Main entry point
"""

import argparse
import json
import re
import os
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


def create_argument_parser():
    """
    Create and configure the command line argument parser.
    
    Returns:
        ArgumentParser configured for the script
    """
    parser = argparse.ArgumentParser(
        description="List modules and their versions from JSON configuration files.",
        epilog="Examples:\n"
               "  %(prog)s                    # List all modules\n"
               "  %(prog)s attrcalc           # List specific module\n"
               "  %(prog)s attrcalc gendata   # List multiple modules",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        'modules',
        nargs='*',
        help='Specific module names to scan (default: scan all modules)'
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
    
    # Print header
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


if __name__ == "__main__":
    main()