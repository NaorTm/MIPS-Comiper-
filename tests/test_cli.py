import subprocess
import tempfile
import unittest
from pathlib import Path

BINARY = Path(__file__).resolve().parents[1] / ('mips.exe' if __import__('os').name == 'nt' else 'mips')

class PipelineSmokeTests(unittest.TestCase):
    def run_trace(self, text, forward='0', branch='0'):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'trace.txt'
            path.write_text(text, encoding='ascii')
            return subprocess.run([str(BINARY), forward, branch, str(path)], text=True, capture_output=True)

    def test_single_instruction_without_final_newline(self):
        result = self.run_trace('1000 add $1 $2 $3')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Instructions: 1\nCycles: 5\nCPI: 5.000', result.stdout.replace('\r\n', '\n'))

    def test_forwarding_reduces_load_use_penalty(self):
        trace = '1000 lw $1 4 $2\n1004 add $3 $1 $2\n'
        slow = self.run_trace(trace, '0')
        fast = self.run_trace(trace, '1')
        self.assertEqual(slow.returncode, 0, slow.stderr)
        self.assertEqual(fast.returncode, 0, fast.stderr)
        def cycles(result): return int(result.stdout.split('Cycles: ')[1].splitlines()[0])
        self.assertGreater(cycles(slow), cycles(fast))

    def test_invalid_trace_and_flags_fail(self):
        self.assertNotEqual(self.run_trace('1000 unknown $1 $2 $3\n').returncode, 0)
        self.assertNotEqual(self.run_trace('1000 add $1 $2 $3\n', 'x').returncode, 0)
        self.assertNotEqual(self.run_trace('1000 add $999 $2 $3\n').returncode, 0)

if __name__ == '__main__': unittest.main()

