#!/usr/bin/env python3

import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional


class TestStatus(Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    TIMEOUT = "TIMEOUT"
    ERROR = "ERROR"


@dataclass
class JobConfig:
    pass_expected: bool = True
    timeout: int = 300

    @classmethod
    def from_string(cls, config_str: str) -> 'JobConfig':
        try:
            # First try direct JSON parsing
            config_dict = json.loads(config_str.lower())
        except (json.JSONDecodeError, ValueError, TypeError):
            try:
                # Try to convert JavaScript-style object to JSON
                # Handle formats like {pass: no, timeout: 300}
                normalized_str = cls._normalize_config_string(config_str)
                config_dict = json.loads(normalized_str.lower())
            except (json.JSONDecodeError, ValueError, TypeError):
                return cls()
        
        return cls(
            pass_expected=config_dict.get('pass', 'yes') in ['yes', 'true', True],
            timeout=int(config_dict.get('timeout', 300))
        )
    
    @classmethod
    def _normalize_config_string(cls, config_str: str) -> str:
        """Convert JavaScript-style object notation to valid JSON."""
        import re
        
        # Remove leading/trailing whitespace
        config_str = config_str.strip()
        
        # Add quotes around unquoted keys
        # Pattern matches: word characters followed by colon
        config_str = re.sub(r'(\w+):', r'"\1":', config_str)
        
        # Add quotes around unquoted string values (but not numbers)
        # Pattern matches: colon followed by whitespace and word characters (not numbers)
        config_str = re.sub(r':\s*([a-zA-Z][a-zA-Z0-9]*)', r': "\1"', config_str)
        
        return config_str


@dataclass
class TestResult:
    job_file: str
    status: TestStatus
    expected_pass: bool
    runtime: float
    timeout_limit: int
    stdout: str = ""
    stderr: str = ""
    error_msg: str = ""

    @property
    def success(self) -> bool:
        if self.expected_pass:
            return self.status == TestStatus.PASS
        else:
            return self.status == TestStatus.FAIL

    @property
    def status_symbol(self) -> str:
        if self.success:
            return "✅"
        elif self.status == TestStatus.TIMEOUT:
            return "⏰"
        elif self.status == TestStatus.ERROR:
            return "💥"
        else:
            return "❌"


class TestEnvironment:
    """Manages test execution environment setup."""
    
    def __init__(self, geodelity_dir: str, debug: bool = False, keep_job_files: bool = False):
        self.geodelity_dir = geodelity_dir
        self.debug = debug
        self.keep_job_files = keep_job_files

    def setup_environment(self) -> Dict[str, str]:
        """Setup environment variables for test execution."""
        env = os.environ.copy()
        
        env['GEODELITY_DIR'] = self.geodelity_dir

        script_dir = Path(__file__).parent
        grun_dir = script_dir.parent / 'grun'

        env['GRUN_DIR'] = str(grun_dir.absolute())
        
        if self.debug:
            env['GDLOGGING_LEVEL'] = 'DEBUG'
        
        if self.keep_job_files:
            env['KEEPJOBFILES'] = 'true'
        
        return env
    
    def validate_geodelity_dir(self) -> bool:
        """Validate that GEODELITY_DIR exists and has required scripts."""
        if not self.geodelity_dir:
            return False
        
        geodelity_path = Path(self.geodelity_dir)
        if not geodelity_path.exists():
            print(f"Error: GEODELITY_DIR path does not exist: {self.geodelity_dir}")
            return False
        
        env_script = geodelity_path / "etc" / "env.sh"
        if not env_script.exists():
            print(f"Error: Environment script not found: {env_script}")
            return False
        
        grun_script = geodelity_path / "bin" / "grun.sh"
        if not grun_script.exists():
            print(f"Error: GRun script not found: {grun_script}")
            return False
        
        return True


class JobConfigParser:
    """Parser for job configuration from job files."""
    
    @staticmethod
    def parse_job_config(job_file: Path) -> JobConfig:
        """Parse job configuration from the first line of a job file."""
        try:
            with open(job_file, 'r', encoding='utf-8') as f:
                first_line = f.readline().strip()
                
            if first_line.startswith('#'):
                config_text = first_line[1:].strip()
                if config_text:
                    return JobConfig.from_string(config_text)
        except (IOError, UnicodeDecodeError):
            pass
        
        return JobConfig()


class SingleTestRunner:
    """Handles running individual test jobs."""
    
    def __init__(self, test_env: TestEnvironment):
        self.test_env = test_env
        self.config_parser = JobConfigParser()
    
    def _print_environment_info(self, env: Dict[str, str]) -> None:
        """Print environment variables information in verbose mode."""
        print("  📋 Environment variables:")
        
        # Key environment variables to display
        key_vars = [
            'GEODELITY_DIR',
            'GRUN_DIR', 
            'GDLOGGING_LEVEL',
            'KEEPJOBFILES'
        ]
        
        for var in key_vars:
            if var in env:
                value = env[var]
                # Truncate very long paths for readability
                if len(value) > 50:
                    display_value = f"...{value[-47:]}"
                else:
                    display_value = value
                print(f"    {var}={display_value}")
        
        print()
    
    def run_test(self, job_file: Path, verbose: bool = False) -> TestResult:
        """Run a single test job file."""
        config = self.config_parser.parse_job_config(job_file)
        
        if verbose:
            print(f"Running {job_file.name} (timeout: {config.timeout}s, expected: {'pass' if config.pass_expected else 'fail'})")
        
        env = self.test_env.setup_environment()
        
        if verbose:
            self._print_environment_info(env)
        
        env_script = f"{self.test_env.geodelity_dir}/etc/env.sh"
        grun_script = f"{self.test_env.geodelity_dir}/bin/grun.sh"
        
        if not Path(env_script).exists():
            return TestResult(
                job_file=job_file.name,
                status=TestStatus.ERROR,
                expected_pass=config.pass_expected,
                runtime=0.0,
                timeout_limit=config.timeout,
                error_msg=f"Environment script not found: {env_script}"
            )
        
        if not Path(grun_script).exists():
            return TestResult(
                job_file=job_file.name,
                status=TestStatus.ERROR,
                expected_pass=config.pass_expected,
                runtime=0.0,
                timeout_limit=config.timeout,
                error_msg=f"GRun script not found: {grun_script}"
            )
        
        command = f'source "{env_script}" && "{grun_script}" "{job_file.name}"'
        
        start_time = time.time()
        try:
            result = subprocess.run(
                ['bash', '-c', command],
                env=env,
                cwd=job_file.parent,
                timeout=config.timeout,
                capture_output=True,
                text=True
            )
            runtime = time.time() - start_time
            
            status = TestStatus.PASS if result.returncode == 0 else TestStatus.FAIL
            
            return TestResult(
                job_file=job_file.name,
                status=status,
                expected_pass=config.pass_expected,
                runtime=runtime,
                timeout_limit=config.timeout,
                stdout=result.stdout,
                stderr=result.stderr
            )
            
        except subprocess.TimeoutExpired as e:
            runtime = time.time() - start_time
            return TestResult(
                job_file=job_file.name,
                status=TestStatus.TIMEOUT,
                expected_pass=config.pass_expected,
                runtime=runtime,
                timeout_limit=config.timeout,
                stdout=e.stdout.decode('utf-8') if e.stdout else "",
                stderr=e.stderr.decode('utf-8') if e.stderr else "",
                error_msg=f"Test timed out after {config.timeout} seconds"
            )
        
        except Exception as e:
            runtime = time.time() - start_time
            return TestResult(
                job_file=job_file.name,
                status=TestStatus.ERROR,
                expected_pass=config.pass_expected,
                runtime=runtime,
                timeout_limit=config.timeout,
                error_msg=str(e)
            )


class TestRunner:
    """Main test runner that coordinates test execution."""
    
    def __init__(self, geodelity_dir: str, debug: bool = False, keep_job_files: bool = False):
        self.test_env = TestEnvironment(geodelity_dir, debug, keep_job_files)
        self.single_runner = SingleTestRunner(self.test_env)
        self.output_formatter = TestOutputFormatter()
    
    def find_job_files(self, tests_dir: Path) -> List[Path]:
        """Find all job files in the tests directory."""
        job_files = []
        for job_file in tests_dir.glob('*.job'):
            job_files.append(job_file)
        return sorted(job_files)
    
    def validate_environment(self) -> bool:
        """Validate test environment before running tests."""
        return self.test_env.validate_geodelity_dir()
    
    def run_all_tests(self, tests_dir: Path, verbose: bool = False) -> List[TestResult]:
        """Run all tests in the given directory."""
        job_files = self.find_job_files(tests_dir)
        if not job_files:
            print(f"No .job files found in {tests_dir}")
            return []
        
        results = []
        total_tests = len(job_files)
        
        if verbose:
            print(f"\nFound {total_tests} test(s) in {tests_dir}")
            print("=" * 60)
        
        for i, job_file in enumerate(job_files, 1):
            if not verbose:
                print(f"[{i}/{total_tests}] {job_file.name}...", end=" ", flush=True)
            
            result = self.single_runner.run_test(job_file, verbose)
            results.append(result)
            
            if not verbose:
                expected_text = "PASS" if result.expected_pass else "FAIL"
                actual_text = result.status.value
                print(f"{result.status_symbol} Expected: {expected_text}, Actual: {actual_text} ({result.runtime:.1f}s)")
            elif verbose:
                self.output_formatter.print_test_result(result)
        
        return results


class TestOutputFormatter:
    """Handles formatting of test output and results."""
    
    def print_test_result(self, result: TestResult) -> None:
        """Print detailed result for a single test."""
        expected_text = "PASS" if result.expected_pass else "FAIL"
        actual_text = result.status.value
        print(f"  Result: {result.status_symbol} Expected: {expected_text}, Actual: {actual_text} ({result.runtime:.1f}s)")
        if result.error_msg:
            print(f"  Error: {result.error_msg}")
        
        # Show full stdout output in verbose mode
        if result.stdout and len(result.stdout.strip()) > 0:
            print(f"  📤 Stdout:")
            # Indent each line of stdout for better readability
            for line in result.stdout.strip().split('\n'):
                print(f"    {line}")
        
        # Show full stderr output in verbose mode
        if result.stderr and len(result.stderr.strip()) > 0:
            print(f"  📥 Stderr:")
            # Indent each line of stderr for better readability
            for line in result.stderr.strip().split('\n'):
                print(f"    {line}")
        
        print()
    
    def print_test_summary(self, results: List[TestResult], verbose: bool = False) -> None:
        """Print summary of all test results."""
        if not results:
            return
        
        total = len(results)
        successful = sum(1 for r in results if r.success)
        failed = total - successful
        
        total_runtime = sum(r.runtime for r in results)
        
        print("\n" + "=" * 60)
        print("TEST SUMMARY")
        print("=" * 60)
        
        print(f"Total tests: {total}")
        print(f"Successful:  {successful} ✅")
        print(f"Failed:      {failed} ❌")
        print(f"Success rate: {successful/total*100:.1f}%")
        print(f"Total runtime: {total_runtime:.1f}s")
        
        if failed > 0:
            print(f"\nFAILED TESTS:")
            for result in results:
                if not result.success:
                    expected_text = "PASS" if result.expected_pass else "FAIL"
                    actual_text = result.status.value
                    reason = f"Expected: {expected_text}, Actual: {actual_text}"
                    if result.error_msg:
                        reason += f" - {result.error_msg}"
                    print(f"  {result.status_symbol} {result.job_file}: {reason} ({result.runtime:.1f}s)")
        
        if verbose and results:
            print(f"\nALL TEST RESULTS:")
            for result in results:
                expected_text = "PASS" if result.expected_pass else "FAIL"
                actual_text = result.status.value
                status_text = "SUCCESS" if result.success else "FAILED"
                print(f"  {result.status_symbol} {result.job_file}: {status_text} - Expected: {expected_text}, Actual: {actual_text} ({result.runtime:.1f}s)")

def validate_geodelity_dir(geodelity_dir: str) -> bool:
    """Backward compatibility function for validating GEODELITY_DIR."""
    env = TestEnvironment(geodelity_dir)
    return env.validate_geodelity_dir()