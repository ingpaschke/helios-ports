"""Native resident-library assembly and dependency-closure regressions."""
from pathlib import Path
import json
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build import Builder
from shared import exports, module_body, private_symbols


class Shared(unittest.TestCase):
    def test_private_names_do_not_alias_between_c_files(self):
        source = '''.public:
 call .local
.local:
 ldc _cache
 ldc _cache_other
 byte ".local _cache"
 global _public
 data _public 1
 data _cache 2
'''
        a = private_symbols(source, ['public'], 'file_a_')
        b = private_symbols(source, ['public'], 'file_b_')
        self.assertIn('call .file_a_local', a)
        self.assertIn('data _file_a_cache 2', a)
        self.assertIn('data _file_b_cache 2', b)
        self.assertIn('.public:', a)
        self.assertIn('ldc _cache_other', a)
        self.assertIn('byte ".local _cache"', a)

    def test_module_composition_keeps_initializers_and_indirect_calls(self):
        source = ''' align
 module -1
.ModStart:
 word #60f160f1
 word .ModEnd-.ModStart
 blkb 31,"sample.c" byte 0
 word modnum
 word 1
 word .MaxData
 init
 align
.public:
 ldl 1
 ldnl 0
 ldnl @_callback
 ldnl _callback
 ret
-- indirect call stubs
..indirect:
 gcall
-- stubs
.external:
 ldl 1
 ldnl 0
 ldnl @_external
 ldnl _external
 gcall
-- initial values
..values:
 byte 7
-- data
 global _public
 data _public 1
-- init routine
 init
 ldc .public-2
 ldpi
 ret
 data .MaxData 0
 align
.ModEnd:
'''
        body = module_body(source)
        self.assertIn('ldnl @_callback\n ldnl _callback', body)
        self.assertIn('..indirect:\n gcall', body)
        self.assertIn('..values:\n byte 7', body)
        self.assertIn('-- init routine\n init\n ldc .public-2', body)
        self.assertNotIn('.external:', body)
        self.assertNotIn('.ModStart', body)
        self.assertNotIn('.ModEnd', body)
        with self.assertRaises(ValueError):
            module_body('.public:\n ret\n')

    def test_function_slots_and_public_data_are_distinct(self):
        with tempfile.TemporaryDirectory() as work:
            obj = Path(work) / 'sample.o'
            obj.with_suffix('.s').write_text('''.public:
 ret
 global _public
 data _public 1
 global _record
 data _record 12
 data _private 1
''')
            functions, data = exports([obj])
            self.assertEqual(functions, ['public'])
            self.assertEqual(data, {'record': 12})

    def test_dependencies_and_aliases_link_once(self):
        with tempfile.TemporaryDirectory() as work:
            a, b, c = [Path(work) / name for name in ('a.def', 'b.def', 'c.def')]
            for p in (a, b, c):
                p.touch()
            alias = Path(work) / 'alias.def'
            alias.symlink_to(b)
            for path, dependencies in ((a, [str(b)]), (b, [str(c)]), (c, [str(a)])):
                Path(str(path) + '.requires.json').write_text(json.dumps(
                    {'format': 1, 'dependencies': dependencies}))
            self.assertEqual(Builder.link_libraries([str(a), str(alias), str(b)]),
                             [str(a), str(b), str(c)])


if __name__ == '__main__':
    unittest.main()
