"""Host checks for FVWM's conservative native-command argument splitter."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / 'ports/fvwm/files/helios/command.c'


class Commands(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory()
        output = Path(cls.work.name) / 'command.so'
        subprocess.run(['cc', '-std=c89', '-Wall', '-Wextra', '-Werror', '-shared',
                        '-fPIC', str(SOURCE), '-o', str(output)], check=True)
        cls.library = ctypes.CDLL(str(output))
        cls.split = cls.library.helios_command_split
        cls.split.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_char_p), ctypes.c_uint]
        cls.split.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    def parse(self, text, capacity=64):
        buffer = ctypes.create_string_buffer(text.encode())
        argv = (ctypes.c_char_p * max(capacity, 1))()
        n = self.split(buffer, argv, capacity)
        if n < 0:
            return None
        self.assertIsNone(argv[n])
        return [argv[i].decode() for i in range(n)]

    def test_literal_arguments(self):
        cases = [('', []), (' \t', []), ('/bin/prog', ['/bin/prog']),
                 (' /bin/prog  one\ttwo ', ['/bin/prog', 'one', 'two']),
                 ('"/path with space/prog" "two words"', ['/path with space/prog', 'two words']),
                 ("/bin/prog '' \"\"", ['/bin/prog', '', '']),
                 ("/bin/prog a' b'c", ['/bin/prog', 'a bc']),
                 ('''/bin/prog "a'b" 'a"b' ''', ['/bin/prog', "a'b", 'a"b']),
                 ("/bin/prog '$HOME; | * \\'", ['/bin/prog', '$HOME; | * \\']),
                 ('/bin/prog "literal ; | & * #"', ['/bin/prog', 'literal ; | & * #']),
                 ('/bin/prog --color=ivory -geometry 80x23+12+76',
                  ['/bin/prog', '--color=ivory', '-geometry', '80x23+12+76'])]
        for text, expected in cases:
            with self.subTest(text=text):
                self.assertEqual(self.parse(text), expected)

    def test_shell_syntax_falls_back(self):
        cases = ['/bin/a | /bin/b', '/bin/a && /bin/b', '/bin/a &', '/bin/a; /bin/b',
                 '/bin/a > out', '/bin/a < in', '/bin/a (x)', '/bin/a *.txt',
                 '/bin/a ?.txt', '/bin/a [ab]', '/bin/a {a,b}', '/bin/a ~/file',
                 '/bin/a # comment', '/bin/a !1', '/bin/a $HOME', '/bin/a "$HOME"',
                 '/bin/a `date`', '/bin/a "`date`"', '/bin/a a\\ b',
                 '/bin/a "a\\b"', '/bin/a\n/bin/b', "/bin/a 'unclosed", '/bin/a "unclosed']
        for text in cases:
            with self.subTest(text=text):
                self.assertIsNone(self.parse(text))

    def test_argument_limit(self):
        self.assertEqual(self.parse('/bin/a b', 3), ['/bin/a', 'b'])
        self.assertIsNone(self.parse('/bin/a b', 2))
        self.assertIsNone(self.parse('', 0))
        self.assertEqual(self.parse(' '.join(['x'] * 63)), ['x'] * 63)
        self.assertIsNone(self.parse(' '.join(['x'] * 64)))


if __name__ == '__main__':
    unittest.main()
