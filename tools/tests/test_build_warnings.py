import contextlib
import importlib.util
import io
import pathlib
import sys
import unittest

SPEC = importlib.util.spec_from_file_location(
    'build_warnings', pathlib.Path(__file__).parents[1] / 'check_build_warnings.py')
bw = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bw)


class WarningLineTest(unittest.TestCase):
    def test_each_toolchain_format(self):
        for line in (
            '/src/core/Tensor.h:856:19: warning: definition of implicit copy constructor [-Wdeprecated-copy]',
            'clang++: warning: argument unused during compilation',
            'ld: warning: ignoring duplicate libraries',
            "C:\\a\\src\\x.cpp(12): warning C4100: 'x': unreferenced parameter",
            "cl : Command line warning D9025 : overriding '/W4' with '/w'",
            'LINK : warning LNK4098: defaultlib conflicts',
            'src/x.cu(1): warning #177-D: variable "u" was declared but never referenced',
            'ptxas /tmp/x.ptx, line 29; warning : Program is doing double precision computations.',
            'nvcc warning : The -std=c++17 flag is not supported',
            'src/x.slang(3): warning 30081: implicit conversion',
            'ninja: warning: multiple rules generate foo',
        ):
            self.assertTrue(bw.is_warning(line), line)

    def test_not_warnings(self):
        for line in (
            '3 warnings generated.',
            'x.cpp:1:2: error: unused variable [-Werror,-Wunused-variable]',
            'cc1plus: all warnings being treated as errors',
            '[12/900] Building CXX object CMakeFiles/spirula.dir/src/app/Warnings.cpp.o',
            '-Wno-error=some-warning: x',
        ):
            self.assertFalse(bw.is_warning(line), line)


class RunTest(unittest.TestCase):
    def main(self, script):
        out = io.StringIO()
        sink = io.BytesIO()

        class Stdout:
            buffer = sink
            write = out.write
            flush = out.flush

        with contextlib.redirect_stdout(Stdout()), contextlib.redirect_stderr(io.StringIO()):
            code = bw.main(['--', sys.executable, '-c', script])
        return code, sink.getvalue().decode(), out.getvalue()

    def test_clean_build_passes(self):
        code, log, _ = self.main('print("[1/1] Linking CXX executable spirula")')
        self.assertEqual(code, 0)
        self.assertIn('Linking', log)

    def test_warning_fails_after_the_build(self):
        code, log, summary = self.main(
            'import sys\n'
            'for _ in range(2): print("a.cpp:1:2: warning: unused [-Wunused]")\n'
            'print("b.cpp:3:4: warning: shadow", file=sys.stderr)\n'
            'print("[9/9] Linking")')
        self.assertEqual(code, 1)
        self.assertIn('[9/9] Linking', log)
        self.assertIn('2 distinct warning lines (3 in all)', summary)

    def test_build_failure_keeps_its_exit_code(self):
        code, _, _ = self.main('import sys; print("x: warning: y"); sys.exit(3)')
        self.assertEqual(code, 3)


if __name__ == '__main__':
    unittest.main()
