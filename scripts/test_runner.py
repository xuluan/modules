#!/usr/bin/env python3

import json
import os
import re
import subprocess
import sys
import time
import yaml
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class TestStatus(Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    TIMEOUT = "TIMEOUT"
    ERROR = "ERROR"


@dataclass
class ModuleInfo:
    name: str
    version: str


@dataclass
class LogMatchResult:
    """结果包含日志匹配的详细信息"""
    patterns: List[str]  # 要匹配的模式列表
    matched_patterns: List[str]  # 成功匹配的模式
    unmatched_patterns: List[str]  # 未匹配的模式
    match_details: Dict[str, List[str]]  # 每个模式匹配到的具体日志行
    
    def __post_init__(self):
        if self.patterns is None:
            self.patterns = []
        if self.matched_patterns is None:
            self.matched_patterns = []
        if self.unmatched_patterns is None:
            self.unmatched_patterns = []
        if self.match_details is None:
            self.match_details = {}
    
    @property
    def all_matched(self) -> bool:
        """返回是否所有模式都匹配成功"""
        return len(self.unmatched_patterns) == 0 and len(self.patterns) > 0
    
    @property
    def has_patterns(self) -> bool:
        """返回是否有需要匹配的模式"""
        return len(self.patterns) > 0

@dataclass
class JobConfig:
    pass_expected: bool = True
    timeout: int = 300
    log_patterns: List[str] = None  # 日志匹配模式
    modules: List[ModuleInfo] = None
    
    def __post_init__(self):
        if self.modules is None:
            self.modules = []
        if self.log_patterns is None:
            self.log_patterns = []

    @classmethod
    def from_string(cls, config_str: str) -> 'JobConfig':
        try:
            # First try direct JSON parsing
            config_dict = json.loads(config_str)
        except (json.JSONDecodeError, ValueError, TypeError):
            try:
                # Try to convert JavaScript-style object to JSON
                # Handle formats like {pass: no, timeout: 300}
                normalized_str = cls._normalize_config_string(config_str)
                config_dict = json.loads(normalized_str)
            except (json.JSONDecodeError, ValueError, TypeError):
                return cls()
        
        # 解析日志匹配模式
        log_patterns = []
        if 'log' in config_dict:
            log_config = config_dict['log']
            if isinstance(log_config, list):
                log_patterns = [str(pattern) for pattern in log_config]
            elif isinstance(log_config, str):
                # 如果是字符串，尝试解析为JSON数组
                try:
                    log_patterns = json.loads(log_config)
                    if not isinstance(log_patterns, list):
                        log_patterns = [str(log_config)]
                except:
                    log_patterns = [str(log_config)]
        
        # 处理pass参数的大小写
        pass_value = config_dict.get('pass', 'yes')
        if isinstance(pass_value, str):
            pass_expected = pass_value.lower() in ['yes', 'true']
        else:
            pass_expected = bool(pass_value)
        
        return cls(
            pass_expected=pass_expected,
            timeout=int(config_dict.get('timeout', 300)),
            log_patterns=log_patterns
        )
    
    @classmethod
    def _normalize_config_string(cls, config_str: str) -> str:
        """Convert JavaScript-style object notation to valid JSON."""
        import re
        
        # Remove leading/trailing whitespace
        config_str = config_str.strip()
        
        # More careful approach: only match keys at the object level, not inside arrays
        # First, temporarily replace array contents to protect them
        array_contents = {}
        array_counter = 0
        
        def replace_array(match):
            nonlocal array_counter
            placeholder = f"__ARRAY_{array_counter}__"
            array_contents[placeholder] = match.group(0)
            array_counter += 1
            return placeholder
        
        # Protect array contents from key replacement
        config_str = re.sub(r'\[[^\[\]]*\]', replace_array, config_str)
        
        # Now safely add quotes around unquoted keys
        config_str = re.sub(r'(\w+):', r'"\1":', config_str)
        
        # Handle common JavaScript values that need quotes
        config_str = re.sub(r':\s*(yes|no)(?=\s*[,}])', r': "\1"', config_str)
        
        # Replace unquoted 'true'/'false' with lowercase
        config_str = re.sub(r':\s*(True|False)(?=\s*[,}])', lambda m: f': {m.group(1).lower()}', config_str)
        
        # Restore array contents
        for placeholder, original in array_contents.items():
            config_str = config_str.replace(placeholder, original)
        
        return config_str


class LogPatternMatcher:
    """日志模式匹配器，支持字符串匹配和正则表达式匹配"""
    
    def __init__(self):
        pass
    
    def _is_regex_pattern(self, pattern: str) -> bool:
        """判断模式是否为正则表达式（以^开头）"""
        return pattern.startswith('^')
    
    def _match_pattern(self, pattern: str, log_text: str) -> List[str]:
        """
        匹配单个模式，返回匹配到的日志行
        
        Args:
            pattern: 要匹配的模式
            log_text: 要搜索的日志文本
            
        Returns:
            匹配到的日志行列表
        """
        matched_lines = []
        
        if not log_text:
            return matched_lines
            
        lines = log_text.split('\n')
        
        if self._is_regex_pattern(pattern):
            # 正则表达式匹配
            try:
                regex_pattern = re.compile(pattern)
                for line in lines:
                    line_stripped = line.strip()
                    if line_stripped and regex_pattern.search(line_stripped):
                        matched_lines.append(line_stripped)
            except re.error:
                # 如果正则表达式无效，回退到字符串匹配
                pattern_cleaned = pattern[1:]  # 移除开头的^
                for line in lines:
                    line_stripped = line.strip()
                    if line_stripped and pattern_cleaned in line_stripped:
                        matched_lines.append(line_stripped)
        else:
            # 字符串匹配
            for line in lines:
                line_stripped = line.strip()
                if line_stripped and pattern in line_stripped:
                    matched_lines.append(line_stripped)
        
        return matched_lines
    
    def _filter_job_setup_content(self, output: str) -> str:
        """
        过滤掉作业配置文件的回显，但保留程序执行输出
        策略：只过滤明确是配置文件内容的行
        """
        lines = output.split('\n')
        filtered_lines = []
        
        # 更保守的策略：只跳过明确的配置文件展示部分
        skip_line = False
        
        for line in lines:
            stripped = line.strip()
            
            # 跳过空行和分隔符
            if not stripped or stripped.startswith('=') or stripped.startswith('-'):
                filtered_lines.append(line)
                continue
                
            # 跳过Job file行和Job Setup标题
            if ('Job file:' in line and '/tests/' in line) or 'Job Setup' in line:
                skip_line = True
                continue
                
            # 检测YAML配置行（以-开头或缩进的键值对）
            if skip_line:
                # 如果是YAML格式的行，跳过
                if (stripped.startswith('-') and ':' in stripped) or \
                   (line.startswith(' ') and ':' in stripped) or \
                   stripped.startswith('#'):
                    continue
                # 遇到分隔符，结束跳过
                elif stripped.startswith('-------'):
                    skip_line = False
                    continue
                # 其他行（如时间戳、构建输出等）保留
                else:
                    skip_line = False
                    filtered_lines.append(line)
            else:
                # 正常情况下保留所有行
                filtered_lines.append(line)
        
        return '\n'.join(filtered_lines)

    def match_patterns(self, patterns: List[str], stdout: str, stderr: str) -> LogMatchResult:
        """
        匹配所有模式
        
        Args:
            patterns: 要匹配的模式列表
            stdout: 标准输出内容
            stderr: 标准错误输出内容
            
        Returns:
            LogMatchResult包含匹配结果详情
        """
        if not patterns:
            return LogMatchResult(
                patterns=[],
                matched_patterns=[],
                unmatched_patterns=[],
                match_details={}
            )
        
        # 使用改进的过滤器
        filtered_stdout = self._filter_job_setup_content(stdout)
        filtered_stderr = self._filter_job_setup_content(stderr)
        
        # 合并过滤后的输出
        combined_log = f"{filtered_stdout}\n{filtered_stderr}"
        
        matched_patterns = []
        unmatched_patterns = []
        match_details = {}
        
        for pattern in patterns:
            matched_lines = self._match_pattern(pattern, combined_log)
            
            if matched_lines:
                matched_patterns.append(pattern)
                match_details[pattern] = matched_lines
            else:
                unmatched_patterns.append(pattern)
                match_details[pattern] = []
        
        return LogMatchResult(
            patterns=patterns.copy(),
            matched_patterns=matched_patterns,
            unmatched_patterns=unmatched_patterns,
            match_details=match_details
        )


@dataclass
class TestResult:
    job_file: str
    status: TestStatus
    expected_pass: bool
    runtime: float
    timeout_limit: int
    modules: List[ModuleInfo] = None
    stdout: str = ""
    stderr: str = ""
    error_msg: str = ""
    log_match_result: LogMatchResult = None  # 日志匹配结果
    
    def __post_init__(self):
        if self.modules is None:
            self.modules = []
        if self.log_match_result is None:
            self.log_match_result = LogMatchResult(
                patterns=[],
                matched_patterns=[],
                unmatched_patterns=[],
                match_details={}
            )

    @property
    def success(self) -> bool:
        # 第一部分：基本的测试执行结果判断
        execution_success = False
        if self.expected_pass:
            execution_success = self.status == TestStatus.PASS
        else:
            execution_success = self.status == TestStatus.FAIL
        
        # 第二部分：日志匹配结果判断（如果有要求的话）
        log_match_success = True  # 默认成功（如果没有日志匹配要求）
        if self.log_match_result and self.log_match_result.has_patterns:
            log_match_success = self.log_match_result.all_matched
        
        # 最终成功 = 执行结果正确 AND 日志匹配成功
        return execution_success and log_match_success

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
        config = JobConfig()
        
        # Parse first line configuration (existing functionality)
        try:
            with open(job_file, 'r', encoding='utf-8') as f:
                first_line = f.readline().strip()
                
            if first_line.startswith('#'):
                config_text = first_line[1:].strip()
                if config_text:
                    config = JobConfig.from_string(config_text)
        except (IOError, UnicodeDecodeError):
            pass
        
        # Parse YAML content for module information
        modules = JobConfigParser.parse_yaml_modules(job_file)
        config.modules = modules
        
        return config
    
    @staticmethod
    def parse_yaml_modules(job_file: Path) -> List[ModuleInfo]:
        """Parse YAML job file to extract module names and versions."""
        modules = []
        
        try:
            with open(job_file, 'r', encoding='utf-8') as f:
                # Skip first line if it's a comment
                content = f.read()
                lines = content.split('\n')
                if lines and lines[0].strip().startswith('#'):
                    yaml_content = '\n'.join(lines[1:])
                else:
                    yaml_content = content
                
            # Parse YAML content
            yaml_data = yaml.safe_load(yaml_content)
            
            if isinstance(yaml_data, list):
                for item in yaml_data:
                    if isinstance(item, dict):
                        # Each item is a dict where the key is the module name
                        # and the value contains the module configuration
                        for module_name, module_config in item.items():
                            if isinstance(module_config, dict):
                                # Look for version field in module config
                                module_version = "unknown"
                                if 'version' in module_config:
                                    module_version = str(module_config['version'])
                                elif 'ver' in module_config:
                                    module_version = str(module_config['ver'])
                                
                                modules.append(ModuleInfo(
                                    name=str(module_name),
                                    version=module_version
                                ))
                            
        except (IOError, UnicodeDecodeError, yaml.YAMLError) as e:
            # If YAML parsing fails, silently continue
            pass
            
        return modules


class SingleTestRunner:
    """Handles running individual test jobs."""
    
    def __init__(self, test_env: TestEnvironment):
        self.test_env = test_env
        self.config_parser = JobConfigParser()
        self.log_matcher = LogPatternMatcher()
    
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
    
    def _perform_log_matching(self, config: JobConfig, stdout: str, stderr: str) -> LogMatchResult:
        """执行日志匹配并返回结果"""
        if config.log_patterns:
            return self.log_matcher.match_patterns(config.log_patterns, stdout, stderr)
        else:
            return LogMatchResult(
                patterns=[],
                matched_patterns=[],
                unmatched_patterns=[],
                match_details={}
            )
    
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
            # 环境脚本不存在的错误情况
            log_match_result = LogMatchResult(
                patterns=config.log_patterns.copy() if config.log_patterns else [],
                matched_patterns=[],
                unmatched_patterns=config.log_patterns.copy() if config.log_patterns else [],
                match_details={}
            )
            
            return TestResult(
                job_file=job_file.name,
                status=TestStatus.ERROR,
                expected_pass=config.pass_expected,
                runtime=0.0,
                timeout_limit=config.timeout,
                modules=config.modules,
                error_msg=f"Environment script not found: {env_script}",
                log_match_result=log_match_result
            )
        
        if not Path(grun_script).exists():
            # GRun脚本不存在的错误情况
            log_match_result = LogMatchResult(
                patterns=config.log_patterns.copy() if config.log_patterns else [],
                matched_patterns=[],
                unmatched_patterns=config.log_patterns.copy() if config.log_patterns else [],
                match_details={}
            )
            
            return TestResult(
                job_file=job_file.name,
                status=TestStatus.ERROR,
                expected_pass=config.pass_expected,
                runtime=0.0,
                timeout_limit=config.timeout,
                modules=config.modules,
                error_msg=f"GRun script not found: {grun_script}",
                log_match_result=log_match_result
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
            
            # 执行日志匹配
            log_match_result = self._perform_log_matching(config, result.stdout, result.stderr)
            
            return TestResult(
                job_file=job_file.name,
                status=status,
                expected_pass=config.pass_expected,
                runtime=runtime,
                timeout_limit=config.timeout,
                modules=config.modules,
                stdout=result.stdout,
                stderr=result.stderr,
                log_match_result=log_match_result
            )
            
        except subprocess.TimeoutExpired as e:
            runtime = time.time() - start_time
            stdout_str = e.stdout.decode('utf-8') if e.stdout else ""
            stderr_str = e.stderr.decode('utf-8') if e.stderr else ""
            
            # 即使超时也尝试进行日志匹配
            log_match_result = self._perform_log_matching(config, stdout_str, stderr_str)
            
            return TestResult(
                job_file=job_file.name,
                status=TestStatus.TIMEOUT,
                expected_pass=config.pass_expected,
                runtime=runtime,
                timeout_limit=config.timeout,
                modules=config.modules,
                stdout=stdout_str,
                stderr=stderr_str,
                error_msg=f"Test timed out after {config.timeout} seconds",
                log_match_result=log_match_result
            )
        
        except Exception as e:
            runtime = time.time() - start_time
            
            # 异常情况下创建空的日志匹配结果
            log_match_result = LogMatchResult(
                patterns=config.log_patterns.copy() if config.log_patterns else [],
                matched_patterns=[],
                unmatched_patterns=config.log_patterns.copy() if config.log_patterns else [],
                match_details={}
            )
            
            return TestResult(
                job_file=job_file.name,
                status=TestStatus.ERROR,
                expected_pass=config.pass_expected,
                runtime=runtime,
                timeout_limit=config.timeout,
                modules=config.modules,
                error_msg=str(e),
                log_match_result=log_match_result
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
                # Show module information
                self.output_formatter._print_test_modules(result)
                # Show log pattern matching results in non-verbose mode too
                self.output_formatter.print_log_match_result(result)
                # Show error logs in non-verbose mode too
                self.output_formatter.print_error_logs(result)
            elif verbose:
                self.output_formatter.print_test_result(result)
        
        return results


class TestOutputFormatter:
    """Handles formatting of test output and results."""
    
    def extract_error_logs(self, output_text: str) -> List[str]:
        """Extract error log lines from output text."""
        if not output_text:
            return []
        
        error_lines = []
        # Patterns to match different types of error logs:
        patterns = [
            # Pattern 1: HH:MM:SS.microseconds [pid] ERROR message
            r'\d{2}:\d{2}:\d{2}\.\d+\s+\[\d+\]\s+(ERROR|FATAL|CRITICAL)\s+.*',
            # Pattern 2: More flexible timestamp with ERROR/FATAL/CRITICAL
            r'\d{2}:\d{2}:\d{2}[.\d]*\s*\[?\d*\]?\s*(ERROR|FATAL|CRITICAL)\s+.*',
            # Pattern 3: Simple ERROR/FATAL/CRITICAL at beginning of line
            r'^(ERROR|FATAL|CRITICAL):\s+.*',
            # Pattern 4: Lines containing "Error:" or "error:"
            r'.*[Ee]rror:\s+.*',
            # Pattern 5: Exception traces
            r'.*(Exception|Error)\s*:.*',
            # Pattern 6: Failed/failure messages
            r'.*(Failed|failed|FAILED)\s+.*'
        ]
        
        for line in output_text.split('\n'):
            line_stripped = line.strip()
            if line_stripped:
                for pattern in patterns:
                    if re.search(pattern, line_stripped):
                        error_lines.append(line_stripped)
                        break  # Don't match same line multiple times
        
        return error_lines
    
    def print_log_match_result(self, result: TestResult) -> None:
        """打印日志匹配结果"""
        if not result.log_match_result or not result.log_match_result.has_patterns:
            return
        
        log_match = result.log_match_result
        
        if log_match.all_matched:
            print(f"  ✅ Log Pattern Match: All {len(log_match.patterns)} pattern(s) matched")
        else:
            print(f"  ❌ Log Pattern Match: {len(log_match.matched_patterns)}/{len(log_match.patterns)} pattern(s) matched")
        
        # 显示匹配的模式
        if log_match.matched_patterns:
            print(f"    ✅ Matched patterns:")
            for pattern in log_match.matched_patterns:
                matched_lines = log_match.match_details.get(pattern, [])
                print(f"      • '{pattern}' → found {len(matched_lines)} match(es)")
                # 显示第一个匹配的示例（截断长行）
                if matched_lines:
                    example = matched_lines[0]
                    if len(example) > 80:
                        example = example[:77] + "..."
                    print(f"        Example: {example}")
        
        # 显示未匹配的模式
        if log_match.unmatched_patterns:
            print(f"    ❌ Unmatched patterns:")
            for pattern in log_match.unmatched_patterns:
                pattern_type = "regex" if pattern.startswith('^') else "string"
                print(f"      • '{pattern}' ({pattern_type})")
        
        print()
    
    def print_error_logs(self, result: TestResult) -> None:
        """Print extracted error logs separately."""
        # Extract errors from both stdout and stderr
        all_errors = []
        all_errors.extend(self.extract_error_logs(result.stdout))
        all_errors.extend(self.extract_error_logs(result.stderr))
        
        if all_errors:
            print(f"  🚨 Error Logs:")
            for error_line in all_errors:
                print(f"    {error_line}")
            print()
    
    def print_test_result(self, result: TestResult) -> None:
        """Print detailed result for a single test."""
        expected_text = "PASS" if result.expected_pass else "FAIL"
        actual_text = result.status.value
        print(f"  Result: {result.status_symbol} Expected: {expected_text}, Actual: {actual_text} ({result.runtime:.1f}s)")
        
        # Show module information
        self._print_test_modules(result)
        
        # Show log pattern matching results
        self.print_log_match_result(result)
        
        if result.error_msg:
            print(f"  Error: {result.error_msg}")
        
        # Show extracted error logs first (most important)
        self.print_error_logs(result)
        
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
    
    def _print_test_modules(self, result: TestResult) -> None:
        """Print module information for a single test."""
        if result.modules:
            modules_info = []
            for module in result.modules:
                modules_info.append(f"{module.name}:{module.version}")
            modules_str = ", ".join(modules_info)
            print(f"  📦 Modules: {modules_str}")
    
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
                    
                    # 添加日志匹配失败信息
                    if result.log_match_result and result.log_match_result.has_patterns and not result.log_match_result.all_matched:
                        unmatched_count = len(result.log_match_result.unmatched_patterns)
                        total_patterns = len(result.log_match_result.patterns)
                        reason += f" - Log match failed: {unmatched_count}/{total_patterns} patterns unmatched"
                    
                    if result.error_msg:
                        reason += f" - {result.error_msg}"
                    print(f"  {result.status_symbol} {result.job_file}: {reason} ({result.runtime:.1f}s)")
        
        # Show error logs summary for failed tests only
        failed_test_error_logs = []
        for result in results:
            if not result.success:  # Only collect errors from failed tests
                errors = self.extract_error_logs(result.stdout)
                errors.extend(self.extract_error_logs(result.stderr))
                if errors:
                    failed_test_error_logs.extend([(result.job_file, error) for error in errors])
        
        if failed_test_error_logs:
            print(f"\n🚨 ERROR LOGS FROM FAILED TESTS ({len(failed_test_error_logs)} total):")
            for job_file, error_line in failed_test_error_logs:
                print(f"  [{job_file}] {error_line}")
        
        if results:
            print(f"\nALL TEST RESULTS:")
            for result in results:
                expected_text = "PASS" if result.expected_pass else "FAIL"
                actual_text = result.status.value
                status_text = "SUCCESS" if result.success else "FAILED"
                
                # Build modules info string
                modules_info = ""
                if result.modules:
                    modules_list = [f"{m.name}:{m.version}" for m in result.modules]
                    modules_str = ", ".join(modules_list)
                    modules_info = f" [📦 {modules_str}]"
                
                print(f"  {result.status_symbol} {result.job_file}: {status_text} - Expected: {expected_text}, Actual: {actual_text} ({result.runtime:.1f}s){modules_info}")

def validate_geodelity_dir(geodelity_dir: str) -> bool:
    """Backward compatibility function for validating GEODELITY_DIR."""
    env = TestEnvironment(geodelity_dir)
    return env.validate_geodelity_dir()