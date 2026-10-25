#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host regressions: run from a prepared XV tree with python3 tests/test_files.py."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(filename, signature):
    source = (ROOT / filename).read_text()
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


class FileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='xv-files-build-')
        cls.addClassCleanup(cls.build.cleanup)
        build = Path(cls.build.name)
        (build / 'upstream.c').write_text(''.join([
            function('xv.c', 'int UncompressFile('),
            function('xv.c', 'void KillPageFiles('),
            function('xvfits.c', 'static int splitfits(image,'),
            function('xvdir.c', 'FILE *OpenOutFile('),
            function('xvbrowse.c', 'static int moveFile(src,dst)'),
        ]))
        # Keep native identity semantics, but use the host's real filesystem.
        subprocess.run([
            'cc', '-std=gnu89', '-Dhelios', '-Dtime=xv_test_time',
            '-I' + str(ROOT / 'tests/helios-stubs'), '-c',
            str(ROOT / 'helios/mktemp.c'), '-o', str(build / 'mktemp.o')
        ], check=True)
        cls.driver = build / 'files-driver'
        # Helios vfork supplies separate descriptor state like fork does here.
        subprocess.run([
            'cc', '-std=gnu89', '-Dvfork=fork', '-I' + str(build),
            str(ROOT / 'tests/files-driver.c'), str(ROOT / 'helios/files.c'),
            str(build / 'mktemp.o'), '-o', str(cls.driver)
        ], check=True)

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='xv-files-')
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def run_driver(self, *args, env=None, success=True):
        result = subprocess.run([str(self.driver), *map(str, args)],
                                env=dict(os.environ, **(env or {})),
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr)
        return result.stdout.strip()

    def test_same_pid_and_timestamp_reserve_distinct_names(self):
        template = str(self.directory / 'xvXXXXXX')
        processes = [subprocess.Popen([str(self.driver), 'name', template],
                                     stdout=subprocess.PIPE, text=True)
                     for _ in range(16)]
        names = []
        for process in processes:
            output, _ = process.communicate()
            self.assertEqual(process.returncode, 0)
            names.append(output.strip())
        self.assertEqual(len(set(names)), len(names))
        self.assertEqual({p.name for p in self.directory.iterdir()},
                         {Path(name).name for name in names})

    def test_native_task_and_machine_affect_names(self):
        template = self.directory / 'xvXXXXXX'
        first = self.run_driver('name', template)
        Path(first).unlink()
        second = self.run_driver('name', template, env={'XV_TEST_PORT': '43'})
        Path(second).unlink()
        third = self.run_driver('name', template, env={'XV_TEST_MACHINE': '/other/cpu'})
        self.assertEqual(len({first, second, third}), 3)

    def test_invalid_template_fails_without_creating_file(self):
        self.assertEqual(self.run_driver('name', self.directory / 'invalid', success=False), '')
        self.assertFalse(list(self.directory.iterdir()))

    def test_missing_directory_fails_without_returning_name(self):
        self.assertEqual(self.run_driver('name', self.directory / 'missing/xvXXXXXX',
                                        success=False), '')

    def test_remove_tree_treats_names_literally_and_preserves_symlink_target(self):
        target = self.directory / 'space ; $(touch marker)'
        target.mkdir()
        (target / 'nested').mkdir()
        (target / 'nested/image').write_text('image')
        (target / '.hidden').write_text('hidden')
        outside = self.directory / 'keep'
        outside.mkdir()
        (outside / 'image').write_text('keep')
        (target / 'link').symlink_to(outside, target_is_directory=True)
        (target / 'dangling').symlink_to(self.directory / 'absent')
        self.run_driver('remove', target)
        self.assertFalse(target.exists())
        self.assertEqual((outside / 'image').read_text(), 'keep')
        self.assertEqual({p.name for p in self.directory.iterdir()}, {'keep'})

    def test_remove_missing_tree_succeeds(self):
        self.run_driver('remove', self.directory / 'absent')

    def test_remove_tree_spans_multiple_directory_buffers(self):
        target = self.directory / 'many entries'
        target.mkdir()
        for i in range(35):
            (target / str(i)).mkdir()
            (target / str(i) / 'image').write_text('image')
        self.run_driver('remove', target)
        self.assertFalse(list(self.directory.iterdir()))

    def test_browser_move_overwrites_literal_directory(self):
        source = self.directory / 'source'
        source.mkdir()
        (source / 'image').write_text('new image')
        destination = self.directory / 'destination ; $(touch marker)'
        destination.mkdir()
        (destination / 'old image').write_text('old image')
        self.run_driver('move', source, destination)
        self.assertFalse(source.exists())
        self.assertEqual((destination / 'image').read_text(), 'new image')
        self.assertEqual({p.name for p in self.directory.iterdir()}, {destination.name})

    def test_browser_cross_filesystem_move_removes_literal_source(self):
        source = self.directory / 'source ; $(touch marker)'
        source.mkdir()
        (source / 'image').write_text('image')
        destination = self.directory / 'destination'
        self.run_driver('move', source, destination, env={'XV_TEST_EXDEV': '1'})
        self.assertFalse(source.exists())
        self.assertTrue(destination.is_dir())
        self.assertEqual({p.name for p in self.directory.iterdir()}, {'destination'})

    def helper(self, body):
        binary = self.directory / 'bin'
        binary.mkdir()
        helper = binary / 'uncompress'
        helper.write_text('#!/bin/sh\n' + body)
        helper.chmod(0o755)
        return {'PATH': str(binary)}

    def test_decompression_uses_path_stdin_and_literal_filenames(self):
        env = self.helper('[ "$#" = 1 ] && [ "$1" = -c ] || exit 3\n/bin/cat\n')
        image = self.directory / 'image name; echo wrong'
        image.write_text('image data')
        suffixed = Path(str(image) + '.Z')
        suffixed.write_text('existing file')
        output_dir = self.directory / 'temp with spaces'
        output_dir.mkdir()
        output = self.run_driver('uncompress', image, output_dir, env=env)
        self.assertEqual(Path(output).read_text(), 'image data')
        self.assertEqual(image.read_text(), 'image data')
        self.assertEqual(suffixed.read_text(), 'existing file')

    def test_failed_decompression_keeps_input_and_removes_partial_output(self):
        env = self.helper('/bin/cat\nexit 9\n')
        image = self.directory / 'image'
        image.write_text('input')
        self.run_driver('uncompress', image, self.directory, env=env, success=False)
        self.assertEqual(image.read_text(), 'input')
        self.assertEqual({p.name for p in self.directory.iterdir()}, {'image', 'bin'})

    def test_missing_decompressor_keeps_input_and_removes_output(self):
        image = self.directory / 'image'
        image.write_text('input')
        self.run_driver('uncompress', image, self.directory,
                        env={'PATH': str(self.directory)}, success=False)
        self.assertEqual({p.name for p in self.directory.iterdir()}, {'image'})

    def test_missing_input_removes_reserved_output(self):
        self.run_driver('uncompress', self.directory / 'absent', self.directory,
                        success=False)
        self.assertFalse(list(self.directory.iterdir()))

    def test_fits_pages_keep_reservation_until_cleanup(self):
        first = self.run_driver('fits', self.directory, 2)
        second = self.run_driver('fits', self.directory, 2)
        self.assertNotEqual(first, second)
        self.assertTrue(Path(first[:-1]).exists())
        self.assertEqual(Path(first + '1').read_bytes(), bytes([10]))
        self.assertEqual(Path(first + '2').read_bytes(), bytes([20]))
        self.run_driver('cleanup', first, 2)
        self.assertTrue(Path(second[:-1]).exists())
        self.run_driver('cleanup', second, 2)
        self.assertFalse(list(self.directory.iterdir()))

    def test_fits_single_page_and_write_failure_release_reservation(self):
        self.run_driver('fits', self.directory, 1)
        self.assertFalse(list(self.directory.iterdir()))
        self.run_driver('fits', self.directory, 2, env={'XV_TEST_HEADER_FAIL': '1'})
        self.assertFalse(list(self.directory.iterdir()))

    def test_pipe_output_uses_reservation_without_overwrite_prompt(self):
        name = self.run_driver('openpipe', self.directory)
        self.assertTrue(Path(name).is_file())


if __name__ == '__main__':
    unittest.main()
