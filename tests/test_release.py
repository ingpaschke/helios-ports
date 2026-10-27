"""Runtime dependency, ABI, filename and archive regressions."""
from pathlib import Path
import json
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import release


def image(name, reference=False, version=1000):
    record = struct.pack('<II32sII', 0x60f260f2 if reference else 0x60f160f1,
                         48, name.encode(), 32, version)
    return struct.pack('<III', 0x12345678, 0, len(record)) + record


class Fixture:
    def __init__(self, work):
        self.work = self.root = work; self.cpu = 't425'; self.linkage = 'shared'

    def config(self, name):
        return {'dependencies': ['library'] if name == 'client' else [],
                'upstream': {'revision': 'pinned'}, 'build': {'targets':
                [{'name': 'Htest', 'type': 'shared'}] if name == 'library' else []}}

    def validate(self, name):
        return {'version': '1', 'inputs': {'cpu': self.cpu, 'linkage': self.linkage}}


class Runtime(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); self.builder = Fixture(self.root)
        self.settings = {'name': 'atari', 'cpu': 't425', 'linkage': 'shared',
                         'short_names': True, 'aliases': {'bin/long-program': 'bin/prog'}}
        self.put('library', 'lib/Htest', image('Htest'))
        self.put('client', 'bin/long-program', image('Htest', True))

    def put(self, port, path, data):
        p = self.root / port / 'stage' / path
        p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(data)
        return p

    def test_dependencies_and_aliases_materialize_without_sdk_or_tests(self):
        self.put('client', 'lib/sdk.a', b'archive')
        self.put('client', 'include/header.h', b'header')
        self.put('client', 'local/libexec/fvwm-protocol-test', b'test')
        self.put('client', 'lib/alias', b'').unlink()
        (self.root / 'client/stage/lib/alias').symlink_to(self.root / 'library/stage/lib/Htest')
        files, metadata = release.collect(self.builder, ['client'], self.settings)
        self.assertEqual(set(files), {'bin/prog', 'lib/Htest', 'lib/alias'})
        self.assertEqual(metadata['core_libraries'], [])
        output = self.root / 'runtime.zip'
        release.archive(self.builder, ['client'], self.settings, output, '1')
        with zipfile.ZipFile(output) as z:
            self.assertEqual(z.read('helios/lib/alias'), image('Htest'))
            self.assertEqual(z.testzip(), None)
        first = output.read_bytes()
        release.archive(self.builder, ['client'], self.settings, output, '1')
        self.assertEqual(first, output.read_bytes())

    def test_missing_resident_is_not_silently_treated_as_a_core_library(self):
        (self.root / 'library/stage/lib/Htest').unlink()
        with self.assertRaisesRegex(ValueError, 'Missing resident'):
            release.collect(self.builder, ['client'], self.settings)

    def test_mixed_abi_rejected(self):
        self.put('library', 'lib/Htest', image('Htest', version=1001))
        with self.assertRaisesRegex(ValueError, 'Incompatible resident'):
            release.collect(self.builder, ['client'], self.settings)

    def test_cpu_and_linkage_are_release_inputs(self):
        for key, value in [('cpu', 't800'), ('linkage', 'static')]:
            with self.assertRaisesRegex(ValueError, 'does not match'):
                release.collect(self.builder, ['client'], dict(self.settings, **{key: value}))

    def test_shortening_collision_and_unmapped_names_are_rejected(self):
        self.put('client', 'bin/another-long-program', b'program')
        with self.assertRaisesRegex(ValueError, 'Not 8.3'):
            release.collect(self.builder, ['client'], self.settings)
        (self.root / 'client/stage/bin/another-long-program').unlink()
        self.put('client', 'local/data/abcdefgh1', b'one')
        self.put('client', 'local/data/abcdefgh2', b'two')
        with self.assertRaisesRegex(ValueError, 'Conflicting'):
            release.collect(self.builder, ['client'], dict(self.settings, shorten_assets=['local/data']))

    def test_executable_modes_survive_distribution_install(self):
        (self.root / 'client/stage/bin/long-program').chmod(0o755)
        release.install(self.builder, ['client'], self.settings, self.root / 'installed')
        self.assertTrue((self.root / 'installed/bin/prog').stat().st_mode & 0o111)

    def test_profile_resources_are_checked_and_included_in_file_hashes(self):
        self.put('client', 'lib/x11/app-defa/XTerm', b'font: unavailable\n')
        path = self.root / 'profiles/atari/XTerm'; path.parent.mkdir(parents=True)
        path.write_text('font: fixed\n')
        settings = dict(self.settings, resources={'lib/x11/app-defa/XTerm': 'atari/XTerm'})
        files, metadata = release.collect(self.builder, ['client'], settings)
        item = files['lib/x11/app-defa/XTerm']
        self.assertEqual(item['source'].read_text(), 'font: fixed\n')
        self.assertEqual(metadata['files']['lib/x11/app-defa/XTerm']['profile_resource'], 'atari/XTerm')
        with self.assertRaisesRegex(ValueError, 'not an installed application default'):
            release.collect(self.builder, ['client'], dict(settings, resources={'bin/prog': 'atari/XTerm'}))


if __name__ == '__main__': unittest.main()
