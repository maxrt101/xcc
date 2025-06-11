from dataclasses import dataclass
import subprocess
import argparse
import json
import re
import os

COLOR_RED    = '\033[31m'
COLOR_GREEN  = '\033[32m'
COLOR_YELLOW = '\033[33m'
COLOR_RESET  = '\033[0m'

@dataclass
class Test:
    @dataclass
    class Expected:
        stdout: list
        stderr: list
        retcode: int | str

        @classmethod
        def from_json(cls, config):
            strings = {'stdout': [], 'stderr': []}
            for out in ['stdout', 'stderr']:
                if out in config:
                    for line in config[out]:
                        strings[out].append(re.compile(line, re.MULTILINE))
            return cls(strings['stdout'], strings['stderr'], config['retcode'])

    id: int
    name: str
    file: str
    expect: Expected

    @classmethod
    def from_json(cls, config):
        return Test(
            config['id'],
            config['name'],
            config['file'],
            cls.Expected.from_json(config['expect'])
        )

@dataclass
class TestRun:
    passed: bool
    retcode: int
    result: int
    stdout: str
    stderr: str
    fail_reasons: list[str]

class Runner:
    RESULT_REGEX = re.compile(r'Result: (\d+)', re.MULTILINE)

    def __init__(self, config: str, executable: str, test_dir: str | None, verbose: bool, print_output: bool):
        self.tests = Runner.__parse(config)
        self.test_dir = test_dir if test_dir else os.path.dirname(config)
        self.executable = executable
        self.verbose = verbose
        self.print_output = print_output
        self.runs = dict()

    def report(self) -> tuple[list[int], list[int]]:
        if not self.runs:
            raise ValueError('Can\'t generate report: no tests ran')

        passed = []
        failed = []

        for id, run in self.runs.items():
            print(f'TEST {COLOR_YELLOW}{id:03}{COLOR_RESET} - ', end='')
            if run.passed:
                passed.append(id)
                print(f'{COLOR_GREEN}PASSED{COLOR_RESET}')
            else:
                failed.append(id)
                print(f'{COLOR_RED}FAILED{COLOR_RESET}')
                print(f'  Failure reasons:')
                for fail_reason in run.fail_reasons:
                    print(f'    {fail_reason}')
                if self.verbose:
                    print(f'  Additional info:')
                    print(f'    name: {self.tests[id].name}')
                    print(f'    file: {self.tests[id].file}')
                    print(f'    retcode: {run.retcode}')
                    print(f'    result: {run.result}')
                    if self.print_output:
                        print(f'    stdout:\n{run.stdout}')
                        print(f'    stderr:\n{run.stderr}')

        return passed, failed

    def run(self, id: int):
        if id not in self.tests:
            raise ValueError(f'Invalid test ID: {id}')

        self.runs[id] = self.__run(self.tests[id])

    def run_range(self, ids: list[int]):
        for id in ids:
            self.run(id)

    def run_all(self):
        self.run_range(list(self.tests.keys()))

    def __run(self, test: Test) -> TestRun:
        result = subprocess.run([self.executable, os.path.join(self.test_dir, test.file)], capture_output=True, text=True)
        run = TestRun(True, result.returncode, 0, result.stdout, result.stderr, [])

        if match := self.RESULT_REGEX.search(run.stdout):
            run.result = int(match.group(1))

        for out in ['stdout', 'stderr']:
            for expect in getattr(test.expect, out):
                if expect.search(run.stdout):
                    continue
                run.fail_reasons.append(f'Couldn\'t find expected string \'{COLOR_YELLOW}{expect.pattern}{COLOR_RESET}\'')
                run.passed = False

        # FIXME: For now, return code isn't returned from xcc process
        if test.expect.retcode != '*' and test.expect.retcode != run.result:
            run.fail_reasons.append(f'Result values mismatch: expected={COLOR_GREEN}{test.expect.retcode}{COLOR_RESET} actual={COLOR_RED}{run.result}{COLOR_RESET}')
            run.passed = False

        return run

    @staticmethod
    def __parse(filename: str) -> dict[int, Test]:
        tests = dict()

        with open(filename) as f:
            config = json.loads(f.read())

        for test in config:
            t = Test.from_json(test)
            tests[t.id] = t

        return tests


def main():
    parser = argparse.ArgumentParser(
         prog='testrun',
         description='XCC Testrun',
         formatter_class=lambda prog: argparse.RawTextHelpFormatter(prog, max_help_position=50)
    )

    parser.add_argument('-c', '--config', action='store', dest='config', required=True,
                        help='Path to config file')

    parser.add_argument('-e', '--executable', action='store', dest='executable', required=True,
                        help='Path to xcc executable')

    parser.add_argument('-d', '--testdir', action='store', dest='testdir', default=None,
                        help='Optional path to tests directory (by default dirname of --config)')

    parser.add_argument('-t', '--tests', action='store', dest='tests', default=None,
                        help='Comma separated list of test ids to run (default: all)')

    parser.add_argument('-v', '--verbose', action='store_true', dest='verbose', default=False,
                        help='Verbose output')

    parser.add_argument('-p', '--print-output', action='store_true', dest='print_output', default=False,
                        help='Print stdout/stderr output')

    args = parser.parse_args()

    runner = Runner(args.config, args.executable, args.testdir, args.verbose, args.print_output)

    if args.tests:
        runner.run_range([int(id) for id in args.tests.split(',')])
    else:
        runner.run_all()

    passed, failed = runner.report()

    print(f'{COLOR_YELLOW}{len(passed)}{COLOR_RESET} tests {COLOR_GREEN}passed{COLOR_RESET}')
    print(f'{COLOR_YELLOW}{len(failed)}{COLOR_RESET} tests {COLOR_RED}failed{COLOR_RESET}')

    if failed:
        if runner.verbose:
            print(f'Failed tests: {" ".join([str(id) for id in failed])}')
        exit(1)

if __name__ == '__main__':
    main()
