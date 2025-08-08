#!/usr/bin/env python3
"""
Test management tool: Object-oriented design for managing test copying and execution.

This tool provides a unified interface for:
- Copying test job files from module directories to central location
- Running tests with comprehensive reporting
- Managing test environments and configurations
"""

import argparse
import os
import sys
import shutil
from pathlib import Path
from typing import List, Optional

from test_copy_utils import TestCopier, ModuleManager, get_all_modules_with_tests
from test_runner import TestRunner, TestOutputFormatter, validate_geodelity_dir


class TestDirectoryManager:
    """Manages test directory operations."""
    
    def __init__(self):
        self.tests_dir = self._get_tests_directory()
        self.grun_dir = self._get_grun_directory()
    
    def _get_tests_directory(self) -> Path:
        """Get the tests directory path relative to the script location."""
        script_dir = Path(__file__).parent
        return script_dir.parent / "tests"

    def _get_grun_directory(self) -> Path:
        """Get the grun directory path relative to the script location."""
        script_dir = Path(__file__).parent
        return script_dir.parent / "grun"        
    
    def clean_directory(self, dry_run: bool = False, verbose: bool = False) -> int:
        """Clean the tests and grun directory by removing all files."""
        count = 0
        if dry_run:
            if verbose:
                print(f"  📋 Would remove: {self.tests_dir}")            
                print(f"  📋 Would remove: {self.grun_dir}") 
            return 2


        if self.tests_dir.exists():
            if verbose:
                print(f"  🗑️  Removed: {self.tests_dir}")               
            shutil.rmtree(self.tests_dir)
            count += 1

        if self.grun_dir.exists():
            if verbose:
                print(f"  🗑️  Removed: {self.grun_dir}")               
            shutil.rmtree(self.grun_dir)
            count += 1

        self.tests_dir.mkdir(parents=True, exist_ok=True)
        self.grun_dir.mkdir(parents=True, exist_ok=True)
        return count

class ArgumentParserBuilder:
    """Builds and configures command line argument parser."""
    
    @staticmethod
    def create_argument_parser() -> argparse.ArgumentParser:
        """Create and configure the command line argument parser."""
        parser = argparse.ArgumentParser(
            description="Test management tool: copy test job files and run tests.",
            epilog="Examples:\n"
                   "  %(prog)s                                    # Copy all test files and run tests\n"
                   "  %(prog)s --modules module1                  # Copy from module1 and run tests\n"
                   "  %(prog)s -m module1 module2                # Copy from multiple modules and run tests\n"
                   "  %(prog)s -m testexpected/test*              # Copy test* files and run tests\n"
                   "  %(prog)s -m testexpected/*1.0.0.job        # Copy *1.0.0.job files and run tests\n"
                   "  %(prog)s -m testexpected/*attr*            # Copy *attr* files and run tests\n"
                   "  %(prog)s --clean                           # Clean tests directory first, then copy and run\n"
                   "  %(prog)s --dry-run                         # Show what would be copied (no tests run)\n"
                   "  %(prog)s --geo /path/to/geodelity           # Specify GEODELITY_DIR for test execution\n"
                   "  %(prog)s --debug --keepjob --verbose       # Run with debug logging, keep job files, and verbose output\n"
                   "\n"
                   "The tool always executes in this order: 1) Copy test files, 2) Run tests",
            formatter_class=argparse.RawDescriptionHelpFormatter
        )
        
        parser.add_argument(
            '--modules', '-m',
            nargs='+',
            metavar='MODULE_SPEC',
            help='Module specifications. Can be module names (e.g., "module1") or '
                 'module with patterns (e.g., "module1/test*", "module1/*1.0.0.job"). '
                 'If not specified, all modules with test directories will be processed.'
        )
        
        parser.add_argument(
            '--clean',
            action='store_true',
            help='Clean the tests directory before copying (removes all files)'
        )
        
        parser.add_argument(
            '--dry-run',
            action='store_true',
            help='Show what would be done without actually copying files'
        )
        
        parser.add_argument(
            '--verbose', '-v',
            action='store_true',
            help='Enable verbose output'
        )
        
        
        parser.add_argument(
            '--geo', '-g',
            metavar='GEODELITY_DIR',
            help='Path to GEODELITY_DIR (required for running tests). '
                 'If not provided, will use environment variable GEODELITY_DIR'
        )
        
        parser.add_argument(
            '--debug', '-d',
            action='store_true',
            help='Enable debug logging (sets GDLOGGING_LEVEL=DEBUG)'
        )
        
        parser.add_argument(
            '--keepjob', '-k',
            action='store_true',
            help='Keep job files after test execution (sets KEEPJOBFILES=true)'
        )
        
        return parser


class TestOutputFormatter:
    """Handles output formatting for test management operations."""
    
    def print_operation_header(self, module_specs: Optional[List[str]], tests_dir: Path, 
                              dry_run: bool = False, clean: bool = False) -> None:
        """Print the header information for the operation (verbose mode only)."""
        mode = "🔍 DRY RUN" if dry_run else "📁 COPY"
        print(f"{mode} - Test File Manager")
        print("=" * 60)
        
        if module_specs:
            print(f"🎯 Target modules: {', '.join(module_specs)}")
        else:
            print("🌟 Processing all modules with test directories")
        
        print(f"📂 Tests directory: {tests_dir}")
        
        if clean:
            clean_mode = " (DRY RUN)" if dry_run else ""
            print(f"🧹 Clean mode: Will remove all existing files{clean_mode}")
        
        if dry_run:
            print("⚠️  This is a dry run - no files will be modified")
        
        print()
    
    def print_copy_summary(self, results: dict, dry_run: bool = False) -> None:
        """Print summary of copy operations."""
        action = "Would be copied" if dry_run else "Copied"
        
        print(f"\n{'='*60}")
        print(f"📊 OPERATION SUMMARY")
        print(f"{'='*60}")
        print(f"📂 Modules processed: {results['processed_modules']}")
        print(f"✅ Files {action.lower()}: {results['copied_files']}")
        
        if results['failed_copies'] > 0:
            print(f"❌ Failed operations: {results['failed_copies']}")
        
        if results['modules_not_found']:
            print(f"⚠️  Modules not found: {', '.join(results['modules_not_found'])}")




class TestRunnerMode:
    """Handles test running mode operations."""
    
    def __init__(self, directory_manager: TestDirectoryManager):
        self.directory_manager = directory_manager
        self.output_formatter = TestOutputFormatter()
    
    def run_tests_mode(self, args) -> None:
        """Execute test running mode."""
        geodelity_dir = self._get_geodelity_dir(args)
        
        if not validate_geodelity_dir(geodelity_dir):
            sys.exit(1)
        
        if args.verbose:
            self._print_run_config(geodelity_dir, args)
        
        # Create test runner and run tests
        runner = TestRunner(geodelity_dir, args.debug, args.keepjob)
        results = runner.run_all_tests(self.directory_manager.tests_dir, args.verbose)
        
        if not results:
            print("❌ No test files found to run")
            sys.exit(1)
        
        # Print test summary
        runner.output_formatter.print_test_summary(results, args.verbose)
        
        # Exit with error code if any tests failed
        failed_count = sum(1 for r in results if not r.success)
        if failed_count > 0:
            sys.exit(1)
    
    def _get_geodelity_dir(self, args) -> str:
        """Get GEODELITY_DIR from args or environment."""
        geodelity_dir = args.geo
        if not geodelity_dir:
            geodelity_dir = os.environ.get('GEODELITY_DIR')
        
        if not geodelity_dir:
            print("❌ Error: GEODELITY_DIR not specified.")
            print("   Use --geo /path/to/geodelity or set GEODELITY_DIR environment variable.")
            sys.exit(1)
        
        return geodelity_dir
    
    def _print_run_config(self, geodelity_dir: str, args) -> None:
        """Print test run configuration."""
        print(f"🔧 GEODELITY_DIR: {geodelity_dir}")
        print(f"📂 Tests directory: {self.directory_manager.tests_dir}")
        print(f"🐛 Debug mode: {'enabled' if args.debug else 'disabled'}")
        print(f"📁 Keep job files: {'enabled' if args.keepjob else 'disabled'}")
        print()


class TestCopyMode:
    """Handles test copying mode operations."""
    
    def __init__(self, directory_manager: TestDirectoryManager):
        self.directory_manager = directory_manager
        self.output_formatter = TestOutputFormatter()
        self.test_copier = TestCopier()
        self.module_manager = ModuleManager()
    
    def copy_tests_mode(self, args) -> None:
        """Execute test copying mode."""
        if args.verbose:
            self._print_debug_info(args)
        
        module_specs = self._get_module_specs(args)
        if not module_specs:
            return
        
        # Print operation header (only in verbose mode)
        if args.verbose:
            self.output_formatter.print_operation_header(
                module_specs, self.directory_manager.tests_dir, args.dry_run, args.clean
            )
        
        # Clean tests directory if requested
        
        self._handle_clean_operation(args)
        if args.clean: # only clean, and quit
            sys.exit(1)
        
        # Copy test files
        self._handle_copy_operation(module_specs, args)
    
    def _print_debug_info(self, args) -> None:
        """Print debug information about received arguments."""
        print(f"🐛 Debug: Received arguments: {args}")
        if args.modules:
            print(f"🐛 Debug: Module specifications: {args.modules}")
            print(f"🐛 Debug: Module spec types: {[type(spec).__name__ for spec in args.modules]}")
    
    def _get_module_specs(self, args) -> Optional[List[str]]:
        """Get module specifications from args or discover all modules."""
        if args.modules:
            return args.modules
        else:
            # Get all modules with test directories
            modules_with_tests = self.module_manager.get_all_modules_with_tests()
            if not modules_with_tests:
                print("❌ No modules with test directories found.")
                return None
            return [name for _, name in modules_with_tests]
    
    def _handle_clean_operation(self, args) -> None:
        """Handle directory cleaning operation."""
        removed_count = self.directory_manager.clean_directory(args.dry_run, args.verbose)
        if removed_count > 0:
            action = "Would remove" if args.dry_run else "Removed"
            print(f"✅ {action} {removed_count} folder(s)")
        else:
            print("✅ No files to remove")
    
    def _handle_copy_operation(self, module_specs: List[str], args) -> None:
        """Handle test file copying operation."""
        results = self.test_copier.copy_module_tests(
            module_specs, self.directory_manager.tests_dir, args.dry_run, args.verbose
        )
        
        if not results:
            print("❌ No modules found to process")
            return
        
        # Print simple summary (always show)
        action = "Would copy" if args.dry_run else "Copied"
        if results['copied_files'] > 0:
            print(f"✅ {action} {results['copied_files']} file(s) from {results['processed_modules']} module(s)")
        
        # Print detailed summary (verbose only)
        if args.verbose:
            self.output_formatter.print_copy_summary(results, args.dry_run)
        
        # Handle errors and warnings
        self._handle_copy_results(results, args)


    def _handle_copy_results(self, results: dict, args) -> None:
        """Handle copy operation results and exit codes."""
        if results['failed_copies'] > 0:
            print(f"❌ {results['failed_copies']} file(s) failed to copy")
            sys.exit(1)
        elif results['copied_files'] == 0 and not args.clean:
            print("⚠️  No files were copied. Check module names and patterns.")
            if results['modules_not_found']:
                print(f"Modules not found: {', '.join(results['modules_not_found'])}")
            sys.exit(1)
        
        if args.verbose:
            print(f"\n🎉 Operation completed successfully!")


class TestManager:
    """Main test management orchestrator."""
    
    def __init__(self):
        self.directory_manager = TestDirectoryManager()
        self.copy_mode = TestCopyMode(self.directory_manager)
        self.runner_mode = TestRunnerMode(self.directory_manager)
        self.argument_parser = ArgumentParserBuilder()
    
    def run(self) -> None:
        """Main entry point for the test manager."""
        parser = self.argument_parser.create_argument_parser()
        args = parser.parse_args()
        
        # Always execute copy first
        self.copy_mode.copy_tests_mode(args)
        
        # Run tests only if not in dry-run mode
        if not args.dry_run:
            self.runner_mode.run_tests_mode(args)


def main():
    """Main function to execute the script."""
    manager = TestManager()
    manager.run()

if __name__ == "__main__":
    main()