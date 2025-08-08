#!/usr/bin/env python3
"""
Test copying utilities: Object-oriented design for copying test files from modules to central directory.

This module contains all the core functionality for:
- Parsing module patterns
- Finding test files
- Copying files with proper naming
- Managing the copy operations
"""

import fnmatch
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Optional


class ModuleManager:
    """Manages module discovery and directory operations."""
    
    def __init__(self):
        self.modules_dir = self._get_modules_directory()
    
    def _get_modules_directory(self) -> Path:
        """Get the modules directory path relative to the script location."""
        script_dir = Path(__file__).parent
        return script_dir.parent / "module"
    
    def get_all_modules_with_tests(self) -> List[Tuple[Path, str]]:
        """Get all modules that have test directories with .job files."""
        if not self.modules_dir.exists():
            return []
        
        modules_with_tests = []
        
        for module_dir in self.modules_dir.iterdir():
            if not module_dir.is_dir():
                continue
                
            test_dir = module_dir / "test"
            if test_dir.exists() and test_dir.is_dir():
                job_files = list(test_dir.glob("*.job"))
                if job_files:
                    modules_with_tests.append((module_dir, module_dir.name))
        
        return modules_with_tests
    
    def get_module_directory(self, module_name: str) -> Optional[Path]:
        """Get the directory path for a specific module."""
        module_dir = self.modules_dir / module_name
        return module_dir if module_dir.exists() else None


class ModulePatternParser:
    """Parser for module specifications including patterns."""
    
    @staticmethod
    def parse_module_pattern(module_spec: str) -> Tuple[str, Optional[str]]:
        """Parse module specification into module name and file pattern."""
        module_spec = str(module_spec).strip()
        
        if not module_spec:
            raise ValueError("Empty module specification")
        
        # Check for potential shell expansion issues
        if module_spec.startswith('./') or module_spec.startswith('/') or '.' in module_spec.split('/')[-1]:
            raise ValueError(f"Looks like shell expanded a wildcard pattern. "
                            f"Use quotes around patterns like 'module/pattern*' to prevent shell expansion. "
                            f"Got: '{module_spec}'")
        
        if '/' not in module_spec:
            # Simple module name like "module1"
            return module_spec, None
        
        # Split into module and pattern parts
        parts = module_spec.split('/', 1)
        module_name = parts[0].strip()
        pattern = parts[1].strip() if len(parts) > 1 else None
        
        if not module_name:
            raise ValueError(f"Invalid module specification: '{module_spec}' - missing module name")
        
        return module_name, pattern


class TestFileManager:
    """Manages finding and handling test files within modules."""
    
    def __init__(self, module_manager: ModuleManager):
        self.module_manager = module_manager
    
    def find_test_files(self, module_dir: Path, module_name: str, pattern: Optional[str] = None) -> List[Path]:
        """Find test job files in a module's test directory."""
        test_dir = module_dir / "test"
        
        if not test_dir.exists() or not test_dir.is_dir():
            return []
        
        job_files = []
        
        for file_path in test_dir.iterdir():
            if not file_path.is_file():
                continue
                
            filename = file_path.name
            
            # Must be a .job file
            if not filename.endswith('.job'):
                continue
            
            # Apply pattern filter if specified
            if pattern:
                if not fnmatch.fnmatch(filename, pattern):
                    continue
            
            job_files.append(file_path)
        
        return sorted(job_files)
    
    @staticmethod
    def generate_target_filename(module_name: str, original_filename: str) -> str:
        """Generate target filename with module prefix."""
        return f"{module_name}__{original_filename}"




class TestFileCopier:
    """Handles copying of test files with proper naming and path management."""
    
    def __init__(self, module_manager: ModuleManager):
        self.module_manager = module_manager
        self.file_manager = TestFileManager(module_manager)
    
    def copy_test_file(self, source_path: Path, target_dir: Path, module_name: str, 
                      dry_run: bool = False, verbose: bool = False) -> bool:
        """Copy a single test file to the target directory with proper naming."""
        original_filename = source_path.name
        target_filename = self.file_manager.generate_target_filename(module_name, original_filename)
        target_path = target_dir / target_filename
        
        if dry_run:
            if verbose:
                source_display = self._get_display_path(source_path)
                target_display = self._get_display_path(target_path)
                print(f"  📋 Would copy: {source_display} -> {target_display}")
            return True
        
        try:
            shutil.copy2(source_path, target_path)
            if verbose:
                print(f"  ✅ Copied: {source_path.name} -> {target_filename}")
            return True
        except Exception as e:
            print(f"  ❌ Failed to copy {source_path.name}: {e}")
            return False
    
    def _get_display_path(self, file_path: Path) -> Path:
        """Get user-friendly path display for a file."""
        try:
            return file_path.relative_to(Path.cwd())
        except ValueError:
            try:
                return file_path.relative_to(self.module_manager.modules_dir.parent)
            except ValueError:
                return file_path.name




class TestCopier:
    """Main orchestrator for copying test files from modules to central directory."""
    
    def __init__(self):
        self.module_manager = ModuleManager()
        self.pattern_parser = ModulePatternParser()
        self.file_copier = TestFileCopier(self.module_manager)
        self.file_manager = TestFileManager(self.module_manager)
    
    def copy_module_tests(self, module_specs: List[str], tests_dir: Path, 
                         dry_run: bool = False, verbose: bool = False) -> Dict[str, any]:
        """Copy test files from specified modules to tests directory."""
        if not self.module_manager.modules_dir.exists():
            print(f"❌ Modules directory not found: {self.module_manager.modules_dir}")
            return {}
        
        # Create tests directory if it doesn't exist
        if not tests_dir.exists() and not dry_run:
            tests_dir.mkdir(parents=True, exist_ok=True)
            if verbose:
                print(f"📁 Created tests directory: {tests_dir}")
        
        results = {
            'processed_modules': 0,
            'copied_files': 0,
            'failed_copies': 0,
            'modules_not_found': []
        }
        
        for module_spec in module_specs:
            self._process_module_spec(module_spec, tests_dir, dry_run, verbose, results)
        
        return results
    
    def _process_module_spec(self, module_spec: str, tests_dir: Path, dry_run: bool, 
                           verbose: bool, results: Dict[str, any]) -> None:
        """Process a single module specification."""
        try:
            module_name, file_pattern = self.pattern_parser.parse_module_pattern(module_spec)
        except ValueError as e:
            print(f"❌ Invalid module specification '{module_spec}': {e}")
            results['modules_not_found'].append(str(module_spec))
            return
        
        module_dir = self.module_manager.get_module_directory(module_name)
        if not module_dir:
            print(f"❌ Module not found: {module_name}")
            results['modules_not_found'].append(module_name)
            return
        
        if verbose:
            print(f"\n📂 Processing module: {module_name}")
            if file_pattern:
                print(f"   🎯 Pattern: {file_pattern}")
        
        # Find test files
        test_files = self.file_manager.find_test_files(module_dir, module_name, file_pattern)
        
        if not test_files:
            if verbose:
                if file_pattern:
                    print(f"   ⚠️  No .job files matching pattern '{file_pattern}' found in {module_name}/test/")
                else:
                    print(f"   ⚠️  No .job files found in {module_name}/test/")
            return
        
        if verbose:
            print(f"   📋 Found {len(test_files)} .job file(s)")
        
        # Copy each file
        results['processed_modules'] += 1
        
        for test_file in test_files:
            success = self.file_copier.copy_test_file(test_file, tests_dir, module_name, dry_run, verbose)
            if success:
                results['copied_files'] += 1
            else:
                results['failed_copies'] += 1


# Backward compatibility functions
def get_modules_directory() -> Path:
    """Backward compatibility function."""
    manager = ModuleManager()
    return manager.modules_dir


def get_all_modules_with_tests() -> List[Tuple[Path, str]]:
    """Backward compatibility function."""
    manager = ModuleManager()
    return manager.get_all_modules_with_tests()


def copy_module_tests(module_specs: List[str], tests_dir: Path, dry_run: bool = False, verbose: bool = False) -> Dict[str, any]:
    """Backward compatibility function."""
    copier = TestCopier()
    return copier.copy_module_tests(module_specs, tests_dir, dry_run, verbose)