# SPDX-License-Identifier: MIT
"""Exercise source reconstruction and build reuse without a Transputer SDK."""
from pathlib import Path
import json
import shutil
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import sources
from build import Builder


class PortsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.work = self.root / 'work'
        self.cache = self.work / 'cache'
        self.cache.mkdir(parents=True)
        self.sdk = self.root / 'sdk'
        self.toolchain = self.root / 'toolchain'
        self.assembler = self.root / 'asm'
        for path in (self.assembler, self.sdk/'include/stdio.h', self.sdk/'lib/c0.o',
                     self.toolchain/'bin/clang', self.toolchain/'bin/hcc',
                     self.toolchain/'sysroots/helios/lib/llvmrt-generic.lib',
                     self.root/'ports.py', self.root/'tools/build.py'):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('fixture input\n')
        self.upstream = self.root/'original'
        self.upstream.mkdir()
        (self.upstream/'hello.txt').write_text('upstream\n')
        self.archive = self.cache/'fixture.tar.gz'
        with tarfile.open(self.archive, 'w:gz') as out:
            out.add(self.upstream, arcname='fixture')
        self.add_port('one')

    def add_port(self, name, dependencies=()):
        directory = self.root/'ports'/name
        directory.mkdir(parents=True)
        config = {'format': 1, 'name': name, 'version': '1',
                  'upstream': {'kind': 'archive', 'url': 'unused:offline-fixture',
                               'filename': self.archive.name, 'sha256': sources.sha(self.archive),
                               'subdirectory': 'fixture', 'tree_sha256': sources.tree_hash(self.upstream)},
                  'patches': [], 'patched_tree_sha256': sources.tree_hash(self.upstream),
                  'dependencies': list(dependencies), 'build': {'install': [
                      {'type': 'copy', 'source': '${src}/hello.txt', 'destination': '${stage}/hello'}]}}
        self.save(name, config)
        return config

    def save(self, name, config):
        (self.root/'ports'/name/'port.json').write_text(json.dumps(config))

    def builder(self):
        return Builder(self.work, self.sdk, self.assembler, toolchain=self.toolchain,
                       root=self.root, offline=True)

    def prepare(self):
        return sources.prepare('one', self.work, self.root, offline=True)

    def test_pristine_and_changed_cache(self):
        tree = self.prepare()
        self.assertEqual(sources.inventory(tree), sources.inventory(self.upstream))
        (tree/'hello.txt').write_text('accidental cache edit')
        self.assertEqual((self.prepare()/'hello.txt').read_text(), 'upstream\n')

    def test_build_diagnostics_ignore_quoted_source(self):
        log = self.work / 'diagnostic.log'
        Builder.run([sys.executable, '-c',
                     'print(\'test.c:1: warning: old declaration\');'
                     'print(\'  77 | error("Fatal Error: example");\');'
                     'print(\'     |       ^\')'], log)
        # Some legacy assemblers report failures without a nonzero status.
        for diagnostic in ('Fatal Error: bad input', 'undefined symbol: missing',
                           'undefined reference to missing'):
            with self.subTest(diagnostic=diagnostic):
                with self.assertRaises(RuntimeError):
                    Builder.run([sys.executable, '-c',
                                 'print(' + repr(diagnostic) + ')'], log)

    def test_new_files_modes_symlinks_and_removal_round_trip(self):
        edited = self.root/'edited'
        shutil.copytree(self.upstream, edited)
        (edited/'hello.txt').unlink()
        (edited/'payload').write_bytes(bytes(range(256))*3)
        (edited/'payload').chmod(0o755)
        (edited/'alias').symlink_to('payload')
        sources.edit_patch('one', edited, self.work, self.root)
        overlay = self.root/'ports/one/files'
        self.assertEqual((overlay/'payload').read_bytes(), bytes(range(256))*3)
        self.assertTrue((overlay/'alias').is_symlink())
        self.assertNotIn('new file mode', (self.root/'ports/one/helios.patch').read_text())
        self.assertEqual(sources.inventory(self.prepare()), sources.inventory(edited))
        sources.edit_patch('one', self.upstream, self.work, self.root)
        self.assertEqual(sources.inventory(self.prepare()), sources.inventory(self.upstream))

    def test_direct_port_file_edits_require_refresh(self):
        edited = self.root/'edited'
        shutil.copytree(self.upstream, edited)
        (edited/'native.c').write_text('original adapter\n')
        sources.edit_patch('one', edited, self.work, self.root)
        self.assertEqual(sources.configuration('one', self.root)['patches'], [])
        self.builder().build('one')
        adapter = self.root/'ports/one/files/native.c'
        adapter.write_text('updated adapter\n')
        with self.assertRaisesRegex(RuntimeError, 'stale'): self.builder().validate('one')
        with self.assertRaisesRegex(RuntimeError, 'refresh'): self.prepare()
        sources.refresh('one', self.work, self.root, offline=True)
        self.assertEqual((self.prepare()/'native.c').read_text(), adapter.read_text())
        self.builder().build('one')

    def test_port_files_cannot_hide_an_upstream_modification(self):
        overlay = self.root/'ports/one/files'
        overlay.mkdir()
        (overlay/'hello.txt').write_text('replacement')
        config = sources.configuration('one', self.root)
        config['files'] = 'files'
        self.save('one', config)
        with self.assertRaisesRegex(RuntimeError, 'must not replace upstream'):
            sources.refresh('one', self.work, self.root, offline=True)

    def test_bad_archive_is_rejected(self):
        with self.archive.open('ab') as out: out.write(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'checksum mismatch'): self.prepare()

    def test_patch_series_preserves_earlier_changes(self):
        edited = self.root/'edited'
        shutil.copytree(self.upstream, edited)
        (edited/'hello.txt').write_text('first adaptation\n')
        sources.edit_patch('one', edited, self.work, self.root,
                           patch_name='01-native.patch', description='Use the native service')
        first = (self.root/'ports/one/01-native.patch').read_bytes()
        (edited/'hello.txt').write_text('first adaptation\nsecond adaptation\n')
        sources.edit_patch('one', edited, self.work, self.root,
                           patch_name='02-resize.patch', description='Handle terminal resizing')
        self.assertEqual(sources.inventory(self.prepare()), sources.inventory(edited))
        (edited/'hello.txt').write_text('first adaptation\nrevised second adaptation\n')
        sources.edit_patch('one', edited, self.work, self.root)
        self.assertEqual(sources.inventory(self.prepare()), sources.inventory(edited))
        self.assertEqual((self.root/'ports/one/01-native.patch').read_bytes(), first)
        with self.assertRaisesRegex(ValueError, 'Only the last patch'):
            sources.edit_patch('one', edited, self.work, self.root, patch_name='01-native.patch')
        (edited/'hello.txt').write_text('first adaptation\n')
        sources.edit_patch('one', edited, self.work, self.root)
        self.assertEqual(len(sources.configuration('one', self.root)['patches']), 1)
        self.assertEqual(sources.inventory(self.prepare()), sources.inventory(edited))

    def test_generated_stdout_is_not_mixed_with_diagnostics(self):
        config = sources.configuration('one', self.root)
        config['build']['prepare'] = [{'type': 'run',
            'command': [sys.executable, '-c', 'import sys; print("generated"); print("diagnostic", file=sys.stderr)'],
            'stdout': '${src}/hello.txt'}]
        self.save('one', config)
        self.builder().build('one')
        self.assertEqual((self.work/'one/stage/hello').read_text(), 'generated\n')
        self.assertIn('diagnostic', (self.work/'one/logs/prepare-0.log').read_text())

    def test_text_templates_install_only_selected_files(self):
        config = sources.configuration('one', self.root)
        config['build']['install'] = [{'type': 'text-copy', 'source': '${src}',
            'pattern': '*.txt', 'strip_suffix': '.txt', 'destination': '${stage}/help',
            'replace': {'upstream': 'native'}}]
        self.save('one', config)
        self.builder().build('one')
        self.assertEqual((self.work/'one/stage/help/hello').read_text(), 'native\n')

    def test_bad_patch_is_rejected_even_with_cached_source(self):
        edited = self.root/'edited'
        shutil.copytree(self.upstream, edited)
        (edited/'hello.txt').write_text('Helios\n')
        sources.edit_patch('one', edited, self.work, self.root)
        self.prepare()
        patch = self.root/'ports/one/helios.patch'
        patch.write_text(patch.read_text()+'\n')
        with self.assertRaisesRegex(RuntimeError, 'Patch checksum'): self.prepare()

    def test_build_reuse_and_sdk_change(self):
        self.builder().build('one')
        record = self.work/'one/build-info.json'
        before = record.stat().st_mtime_ns
        self.builder().build('one')
        self.assertEqual(before, record.stat().st_mtime_ns)
        (self.sdk/'include/stdio.h').write_text('new header')
        with self.assertRaisesRegex(RuntimeError, 'stale'): self.builder().validate('one')
        self.builder().build('one')
        self.builder().validate('one')

    def test_stage_and_mode_changes_are_detected(self):
        self.builder().build('one')
        stage = self.work/'one/stage'
        (stage/'surprise').write_text('extra')
        with self.assertRaisesRegex(RuntimeError, 'installation stage'): self.builder().validate('one')
        self.builder().build('one')
        self.assertFalse((stage/'surprise').exists())
        (stage/'hello').chmod(0o755)
        with self.assertRaisesRegex(RuntimeError, 'Modified port output'): self.builder().validate('one')

    def test_cpu_change_rebuilds_cached_port(self):
        self.builder().build('one')
        target = Builder(self.work, self.sdk, self.assembler,
                         toolchain=self.toolchain, root=self.root,
                         offline=True, cpu='t425')
        with self.assertRaisesRegex(RuntimeError, 'stale'):
            target.validate('one')
        target.build('one')
        target.validate('one')
        with self.assertRaisesRegex(RuntimeError, 'stale'):
            self.builder().validate('one')

    def test_linkage_change_rebuilds_cached_port(self):
        self.builder().build('one')
        target = Builder(self.work, self.sdk, self.assembler, toolchain=self.toolchain,
                         root=self.root, offline=True, linkage='static')
        with self.assertRaisesRegex(RuntimeError, 'stale'): target.validate('one')
        target.build('one'); target.validate('one')

    def test_conditional_install_does_not_keep_previous_variant_files(self):
        config = sources.configuration('one', self.root)
        config['build']['install'] = [
            {'type': 'copy', 'source': '${src}/hello.txt', 'destination': '${stage}/shared',
             'when': {'linkage': 'shared'}},
            {'type': 'copy', 'source': '${src}/hello.txt', 'destination': '${stage}/static',
             'when': {'linkage': 'static'}}]
        self.save('one', config); self.builder().build('one')
        self.assertTrue((self.work/'one/stage/shared').is_file())
        target = Builder(self.work, self.sdk, self.assembler, toolchain=self.toolchain,
                         root=self.root, offline=True, linkage='static')
        target.build('one')
        self.assertFalse((self.work/'one/stage/shared').exists())
        self.assertTrue((self.work/'one/stage/static').is_file())

    def test_stale_dependency_and_cycle_are_detected(self):
        self.add_port('two', ['one'])
        self.builder().build('two')
        (self.work/'one/stage/hello').write_text('modified dependency')
        with self.assertRaisesRegex(RuntimeError, 'Modified port output'): self.builder().validate('two')
        self.builder().build('two')
        config = sources.configuration('one', self.root)
        config['dependencies'] = ['two']
        self.save('one', config)
        with self.assertRaisesRegex(ValueError, 'cycle'): self.builder().build('two')
        with self.assertRaisesRegex(RuntimeError, 'cycle'): self.builder().validate('two')

    def test_failed_rebuild_invalidates_old_record(self):
        self.builder().build('one')
        config = sources.configuration('one', self.root)
        config['build']['prepare'] = [{'type': 'run', 'command': [sys.executable, '-c', 'raise SystemExit(1)']}]
        self.save('one', config)
        with self.assertRaises(RuntimeError): self.builder().build('one', force=True)
        self.assertFalse((self.work/'one/build-info.json').exists())

    def test_missing_declared_output_fails_build(self):
        config = sources.configuration('one', self.root)
        config['build']['outputs'] = ['${work}/missing']
        self.save('one', config)
        with self.assertRaisesRegex(RuntimeError, 'Missing declared output'): self.builder().build('one')
        self.assertFalse((self.work/'one/build-info.json').exists())

    def test_rebuild_replaces_old_generated_symlinks(self):
        work = self.work/'one'
        work.mkdir()
        (work/'include').symlink_to('src', target_is_directory=True)
        config = sources.configuration('one', self.root)
        config['build']['prepare'] = [
            {'type': 'copy', 'source': '${src}', 'destination': '${work}/include'}]
        self.save('one', config)
        self.builder().build('one')
        self.assertFalse((work/'include').is_symlink())
        self.assertEqual((work/'include/hello.txt').read_text(), 'upstream\n')


if __name__ == '__main__': unittest.main()
