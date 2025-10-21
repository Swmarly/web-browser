#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for eval_prompts."""

import io
import itertools
import os
import pathlib
import subprocess
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

import eval_prompts
import results

# pylint: disable=protected-access


class CheckUncommittedChangesUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the `_check_uncommitted_changes` function."""

    def setUp(self):
        self.setUpPyfakefs()

    @mock.patch('subprocess.run')
    def test_check_uncommitted_changes_clean(self, mock_run):
        """Tests that no warning is issued for a clean checkout."""
        mock_run.return_value = subprocess.CompletedProcess(
            args=['git', 'status', '--porcelain'], returncode=0, stdout='')
        self.fs.create_dir('/tmp/src/out/Default')
        with self.assertNoLogs():
            eval_prompts._check_uncommitted_changes('/tmp/src')

    @mock.patch('subprocess.run')
    def test_check_uncommitted_changes_dirty(self, mock_run):
        """Tests that a warning is issued for a dirty checkout."""
        mock_run.return_value = subprocess.CompletedProcess(
            args=['git', 'status', '--porcelain'],
            returncode=0,
            stdout=' M some_file.py')
        with self.assertLogs(level='WARNING') as cm:
            eval_prompts._check_uncommitted_changes('/tmp/src')
            self.assertIn(
                'Warning: There are uncommitted changes in the repository.',
                cm.output[0])

    @mock.patch('subprocess.run')
    def test_check_uncommitted_changes_extra_out_dir(self, mock_run):
        """Tests that a warning is issued for extra directories in out."""
        mock_run.return_value = subprocess.CompletedProcess(
            args=['git', 'status', '--porcelain'], returncode=0, stdout='')
        self.fs.create_dir('/tmp/src/out/Default')
        self.fs.create_dir('/tmp/src/out/Release')
        self.fs.create_dir('/tmp/src/out/Debug')

        with self.assertLogs(level='WARNING') as cm:
            eval_prompts._check_uncommitted_changes('/tmp/src')
            self.assertIn(
                'Warning: The out directory contains unexpected directories',
                cm.output[0])


class BuildChromiumUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the `_build_chromium` function."""

    def setUp(self):
        self.setUpPyfakefs()

    @mock.patch('subprocess.check_call')
    def test_build_chromium(self, mock_check_call):
        """Tests that the correct commands are called to build chromium."""
        eval_prompts._build_chromium('/tmp/src')
        mock_check_call.assert_has_calls([
            mock.call(
                ['gn', 'gen', 'out/Default', '--args=use_remoteexec=true'],
                cwd='/tmp/src'),
            mock.call(['autoninja', '-C', 'out/Default'], cwd='/tmp/src'),
        ])


class DiscoverTestcaseFilesUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the `_discover_testcase_files` function."""

    def setUp(self):
        self.setUpPyfakefs()

    @mock.patch('eval_prompts.constants.CHROMIUM_SRC',
                pathlib.Path('/chromium/src'))
    def test_discover_testcase_files(self):
        """Tests that testcase files are discovered correctly."""
        self.fs.create_file(
            '/chromium/src/agents/extensions/ext1/tests/test1.promptfoo.yaml')
        self.fs.create_file('/chromium/src/agents/extensions/ext2/tests/sub/'
                            'test2.promptfoo.yaml')
        self.fs.create_file(
            '/chromium/src/agents/prompts/eval/test3.promptfoo.yaml')
        self.fs.create_file(
            '/chromium/src/agents/prompts/eval/sub/test4.promptfoo.yaml')
        self.fs.create_file('/chromium/src/agents/prompts/eval/test5.yaml')

        expected_files = [
            pathlib.Path('/chromium/src/agents/extensions/ext1/tests/'
                         'test1.promptfoo.yaml'),
            pathlib.Path('/chromium/src/agents/extensions/ext2/tests/sub/'
                         'test2.promptfoo.yaml'),
            pathlib.Path(
                '/chromium/src/agents/prompts/eval/test3.promptfoo.yaml'),
            pathlib.Path(
                '/chromium/src/agents/prompts/eval/sub/test4.promptfoo.yaml'),
        ]

        found_files = eval_prompts._discover_testcase_files()
        # We need to convert to strings before comparing since pathlib.Paths
        # created using pyfakefs are different than those created manually even
        # if they refer to the same path.
        self.assertCountEqual([str(p) for p in found_files],
                              [str(p) for p in expected_files])



class DetermineShardValuesUnittest(unittest.TestCase):
    """Unit tests for the `_determine_shard_values` function."""

    @mock.patch.dict(os.environ, {}, clear=True)
    def test_no_args_no_env(self):
        """Tests that the default values are returned w/o shard info."""
        self.assertEqual(eval_prompts._determine_shard_values(None, None),
                         (0, 1))

    def test_args_provided(self):
        """Tests that the argument values are used when provided."""
        self.assertEqual(eval_prompts._determine_shard_values(1, 3), (1, 3))
        self.assertEqual(eval_prompts._determine_shard_values(0, 1), (0, 1))

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '2',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '4'
    },
                     clear=True)
    def test_env_vars_provided(self):
        """Tests that the env variable values are used when provided."""
        self.assertEqual(eval_prompts._determine_shard_values(None, None),
                         (2, 4))

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '2',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '4'
    },
                     clear=True)
    def test_args_and_env_vars_provided(self):
        """Tests that arg values take precedence over environment variables."""
        with self.assertLogs(level='WARNING') as cm:
            self.assertEqual(eval_prompts._determine_shard_values(1, 3),
                             (1, 3))
            self.assertIn(
                'WARNING:root:Shard index set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)
            self.assertIn(
                'WARNING:root:Total shards set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)

    def test_shard_index_arg_only(self):
        """Tests that ValueError is raised if only shard_index is provided."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(1, None)

    def test_total_shards_arg_only(self):
        """Tests that ValueError is raised if only total_shards is provided."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(None, 3)

    @mock.patch.dict(os.environ, {eval_prompts._SHARD_INDEX_ENV_VAR: '1'},
                     clear=True)
    def test_shard_index_env_only(self):
        """Tests that a ValueError is raised if only shard_index is in env."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(None, None)

    @mock.patch.dict(os.environ, {eval_prompts._TOTAL_SHARDS_ENV_VAR: '3'},
                     clear=True)
    def test_total_shards_env_only(self):
        """Tests that a ValueError is raised if only total_shards is in env."""
        with self.assertRaisesRegex(
                ValueError, 'Only one of shard index or total shards was set'):
            eval_prompts._determine_shard_values(None, None)

    def test_negative_shard_index(self):
        """Tests that a ValueError is raised for a negative shard_index."""
        with self.assertRaisesRegex(ValueError,
                                    'Shard index must be non-negative'):
            eval_prompts._determine_shard_values(-1, 3)

    def test_zero_total_shards(self):
        """Tests that a ValueError is raised for a total_shards of zero."""
        with self.assertRaisesRegex(ValueError,
                                    'Total shards must be positive'):
            eval_prompts._determine_shard_values(0, 0)

    def test_negative_total_shards(self):
        """Tests that a ValueError is raised for a negative total_shards."""
        with self.assertRaisesRegex(ValueError,
                                    'Total shards must be positive'):
            eval_prompts._determine_shard_values(0, -1)

    def test_shard_index_equal_to_total_shards(self):
        """Tests that a ValueError is raised if shard_index == total_shards."""
        with self.assertRaisesRegex(ValueError,
                                    'Shard index must be < total shards'):
            eval_prompts._determine_shard_values(3, 3)

    def test_shard_index_greater_than_total_shards(self):
        """Tests that a ValueError is raised if shard_index > total_shards."""
        with self.assertRaisesRegex(ValueError,
                                    'Shard index must be < total shards'):
            eval_prompts._determine_shard_values(4, 3)

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '1',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '5'
    },
                     clear=True)
    def test_total_shards_from_args_shard_index_from_env(self):
        """Tests values are picked up from args and env correctly."""
        with self.assertLogs(level='WARNING') as cm:
            self.assertEqual(eval_prompts._determine_shard_values(None, 3),
                             (1, 3))
            self.assertIn(
                'WARNING:root:Total shards set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)

    @mock.patch.dict(os.environ, {
        eval_prompts._SHARD_INDEX_ENV_VAR: '1',
        eval_prompts._TOTAL_SHARDS_ENV_VAR: '5'
    },
                     clear=True)
    def test_shard_index_from_args_total_shards_from_env(self):
        """Tests values are picked up from args and env correctly."""
        with self.assertLogs(level='WARNING') as cm:
            self.assertEqual(eval_prompts._determine_shard_values(2, None),
                             (2, 5))
            self.assertIn(
                'WARNING:root:Shard index set by both arguments and '
                'environment variable. Using value provided by arguments.',
                cm.output)


class GetTestsToRunUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the `_get_tests_to_run` function."""

    def setUp(self):
        self.setUpPyfakefs()
        discover_patcher = mock.patch('eval_prompts._discover_testcase_files')
        self.mock_discover_testcase_files = discover_patcher.start()
        self.addCleanup(discover_patcher.stop)

        determine_shard_patcher = mock.patch(
            'eval_prompts._determine_shard_values')
        self.mock_determine_shard_values = determine_shard_patcher.start()
        self.addCleanup(determine_shard_patcher.stop)

        constants_patcher = mock.patch('eval_prompts.constants.CHROMIUM_SRC',
                                       pathlib.Path('/chromium/src'))
        self.mock_constants = constants_patcher.start()
        self.addCleanup(constants_patcher.stop)

    def test_get_tests_to_run_no_sharding_no_filter(self):
        """Tests that all tests are returned with no sharding or filtering."""
        self.mock_determine_shard_values.return_value = (0, 1)
        self.mock_discover_testcase_files.return_value = [
            pathlib.Path('/chromium/src/test/a.yaml'),
            pathlib.Path('/chromium/src/test/b.yaml'),
            pathlib.Path('/chromium/src/test/c.yaml'),
        ]

        result = eval_prompts._get_tests_to_run(None, None, None)
        self.assertEqual(len(result), 3)
        self.assertIn(pathlib.Path('/chromium/src/test/a.yaml'), result)
        self.assertIn(pathlib.Path('/chromium/src/test/b.yaml'), result)
        self.assertIn(pathlib.Path('/chromium/src/test/c.yaml'), result)

    def test_get_tests_to_run_with_filter(self):
        """Tests that tests are filtered correctly."""
        self.mock_determine_shard_values.return_value = (0, 1)
        self.mock_discover_testcase_files.return_value = [
            pathlib.Path('/chromium/src/test/a.yaml'),
            pathlib.Path('/chromium/src/test/b.yaml'),
            pathlib.Path('/chromium/src/test/c.yaml'),
        ]

        result = eval_prompts._get_tests_to_run(None, None, '*/b.yaml')
        self.assertEqual(len(result), 1)
        self.assertIn(pathlib.Path('/chromium/src/test/b.yaml'), result)

    def test_get_tests_to_run_with_multiple_filters(self):
        """Tests that tests are filtered correctly with multiple filters."""
        self.mock_determine_shard_values.return_value = (0, 1)
        self.mock_discover_testcase_files.return_value = [
            pathlib.Path('/chromium/src/test/a.yaml'),
            pathlib.Path('/chromium/src/test/b.yaml'),
            pathlib.Path('/chromium/src/test/c.yaml'),
        ]

        result = eval_prompts._get_tests_to_run(None, None,
                                                '*/a.yaml::*/c.yaml')
        self.assertEqual(len(result), 2)
        self.assertIn(pathlib.Path('/chromium/src/test/a.yaml'), result)
        self.assertIn(pathlib.Path('/chromium/src/test/c.yaml'), result)

    def test_get_tests_to_run_with_sharding(self):
        """Tests that tests are sharded correctly."""
        self.mock_determine_shard_values.return_value = (1, 2)
        self.mock_discover_testcase_files.return_value = [
            pathlib.Path('/chromium/src/test/a.yaml'),
            pathlib.Path('/chromium/src/test/b.yaml'),
            pathlib.Path('/chromium/src/test/c.yaml'),
            pathlib.Path('/chromium/src/test/d.yaml'),
        ]

        result = eval_prompts._get_tests_to_run(1, 2, None)
        self.assertEqual(len(result), 2)
        # The list is sorted before sharding
        self.assertIn(pathlib.Path('/chromium/src/test/b.yaml'), result)
        self.assertIn(pathlib.Path('/chromium/src/test/d.yaml'), result)

    def test_get_tests_to_run_with_sharding_and_filter(self):
        """Tests that tests are filtered and then sharded correctly."""
        self.mock_determine_shard_values.return_value = (0, 2)
        self.mock_discover_testcase_files.return_value = [
            pathlib.Path('/chromium/src/test/a.yaml'),
            pathlib.Path('/chromium/src/test/b.yaml'),
            pathlib.Path('/chromium/src/test/c.yaml'),
            pathlib.Path('/chromium/src/test/d_filtered.yaml'),
            pathlib.Path('/chromium/src/test/e_filtered.yaml'),
        ]

        result = eval_prompts._get_tests_to_run(0, 2, '*filtered*')
        self.assertEqual(len(result), 1)
        self.assertIn(pathlib.Path('/chromium/src/test/d_filtered.yaml'),
                      result)

    def test_get_tests_to_run_no_tests_found(self):
        """Tests that an empty list is returned when no tests are found."""
        self.mock_determine_shard_values.return_value = (0, 1)
        self.mock_discover_testcase_files.return_value = []

        result = eval_prompts._get_tests_to_run(None, None, None)
        self.assertEqual(len(result), 0)


class ReadPassKConfigUnittest(fake_filesystem_unittest.TestCase):
    """Unit tests for the `_read_pass_k_config` function."""

    def setUp(self):
        """Sets up the fake filesystem."""
        self.setUpPyfakefs()

    def test_empty_config(self):
        """Tests that default values are returned for an empty config."""
        self.fs.create_file('test.yaml', contents='{}')
        config = eval_prompts._read_pass_k_config(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 1)
        self.assertEqual(config.pass_k_threshold, 1)

    def test_no_tests_key(self):
        """Tests that default values are returned when 'tests' is missing."""
        self.fs.create_file('test.yaml', contents='foo: bar')
        config = eval_prompts._read_pass_k_config(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 1)
        self.assertEqual(config.pass_k_threshold, 1)

    def test_empty_tests_list(self):
        """Tests that default values are returned for an empty 'tests' list."""
        self.fs.create_file('test.yaml', contents='tests: []')
        config = eval_prompts._read_pass_k_config(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 1)
        self.assertEqual(config.pass_k_threshold, 1)

    def test_no_metadata(self):
        """Tests that default values are returned for tests with no metadata."""
        self.fs.create_file('test.yaml', contents='tests:\n  - foo: bar')
        config = eval_prompts._read_pass_k_config(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 1)
        self.assertEqual(config.pass_k_threshold, 1)

    def test_empty_metadata(self):
        """Tests that default values are returned for empty metadata."""
        self.fs.create_file('test.yaml', contents='tests:\n  - metadata: {}')
        config = eval_prompts._read_pass_k_config(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 1)
        self.assertEqual(config.pass_k_threshold, 1)

    def test_with_settings(self):
        """Tests that pass@k settings are read correctly."""
        yaml_with_settings = """
tests:
  - metadata:
      runs_per_test: 5
      pass_k_threshold: 3
"""
        self.fs.create_file('test.yaml', contents=yaml_with_settings)
        config = eval_prompts._read_pass_k_config(pathlib.Path('test.yaml'))
        self.assertEqual(config.runs_per_test, 5)
        self.assertEqual(config.pass_k_threshold, 3)

    def test_first_test_has_settings(self):
        """Tests that settings are read from the first test with metadata."""
        yaml_first_test_has_settings = """
tests:
  - metadata:
      runs_per_test: 5
      pass_k_threshold: 3
  - metadata:
      runs_per_test: 10
      pass_k_threshold: 8
"""
        self.fs.create_file('test.yaml', contents=yaml_first_test_has_settings)
        with self.assertLogs(level='WARNING') as cm:
            config = eval_prompts._read_pass_k_config(
                pathlib.Path('test.yaml'))
            self.assertIn('Settings on other tests will be ignored',
                          cm.output[0])
        self.assertEqual(config.runs_per_test, 5)
        self.assertEqual(config.pass_k_threshold, 3)

    def test_later_test_has_settings(self):
        """Tests that settings are read from the first test with metadata."""
        yaml_later_test_has_settings = """
tests:
  - {}
  - metadata:
      runs_per_test: 5
      pass_k_threshold: 3
"""
        self.fs.create_file('test.yaml', contents=yaml_later_test_has_settings)
        with self.assertLogs(level='WARNING') as cm:
            config = eval_prompts._read_pass_k_config(
                pathlib.Path('test.yaml'))
            self.assertIn('Settings on other tests will be ignored',
                          cm.output[0])
        self.assertEqual(config.runs_per_test, 1)
        self.assertEqual(config.pass_k_threshold, 1)

    def test_invalid_runs_type(self):
        """Tests that a ValueError is raised for a non-integer runs_per_test."""
        yaml_invalid_runs = """
tests:
  - metadata:
      runs_per_test: "5"
"""
        self.fs.create_file('test.yaml', contents=yaml_invalid_runs)
        with self.assertRaisesRegex(ValueError, 'must be an integer'):
            eval_prompts._read_pass_k_config(pathlib.Path('test.yaml'))

    def test_invalid_threshold_type(self):
        """Tests that a ValueError is raised for a non-integer value."""
        yaml_invalid_threshold = """
tests:
  - metadata:
      pass_k_threshold: 3.5
"""
        self.fs.create_file('test.yaml', contents=yaml_invalid_threshold)
        with self.assertRaisesRegex(ValueError, 'must be an integer'):
            eval_prompts._read_pass_k_config(pathlib.Path('test.yaml'))


class PerformChromiumSetupUnittest(unittest.TestCase):
    """Unit tests for the `_perform_chromium_setup` function."""

    @mock.patch('eval_prompts._build_chromium')
    @mock.patch('eval_prompts._check_uncommitted_changes')
    @mock.patch('subprocess.run')
    @mock.patch('checkout_helpers.check_btrfs')
    @mock.patch('checkout_helpers.get_gclient_root')
    def test_perform_chromium_setup_build_btrfs(self, mock_get_gclient_root,
                                                mock_check_btrfs,
                                                mock_subprocess_run,
                                                mock_check_uncommitted_changes,
                                                mock_build_chromium):
        """Tests setup with build and btrfs."""
        mock_get_gclient_root.return_value = pathlib.Path('/root')
        mock_check_btrfs.return_value = True

        eval_prompts._perform_chromium_setup(force=False, build=True)

        mock_get_gclient_root.assert_called_once()
        mock_check_btrfs.assert_called_once_with(pathlib.Path('/root'))
        mock_subprocess_run.assert_called_once_with(['sudo', '-v'], check=True)
        mock_check_uncommitted_changes.assert_called_once_with(
            pathlib.Path('/root/src'))
        mock_build_chromium.assert_called_once_with(pathlib.Path('/root/src'))

    @mock.patch('eval_prompts._build_chromium')
    @mock.patch('eval_prompts._check_uncommitted_changes')
    @mock.patch('subprocess.run')
    @mock.patch('checkout_helpers.check_btrfs')
    @mock.patch('checkout_helpers.get_gclient_root')
    def test_perform_chromium_setup_no_build_no_btrfs(
            self, mock_get_gclient_root, mock_check_btrfs, mock_subprocess_run,
            mock_check_uncommitted_changes, mock_build_chromium):
        """Tests setup without build and without btrfs."""
        mock_get_gclient_root.return_value = pathlib.Path('/root')
        mock_check_btrfs.return_value = False

        eval_prompts._perform_chromium_setup(force=False, build=False)

        mock_get_gclient_root.assert_called_once()
        mock_check_btrfs.assert_called_once_with(pathlib.Path('/root'))
        mock_subprocess_run.assert_not_called()
        mock_check_uncommitted_changes.assert_called_once_with(
            pathlib.Path('/root/src'))
        mock_build_chromium.assert_not_called()

    @mock.patch('eval_prompts._build_chromium')
    @mock.patch('eval_prompts._check_uncommitted_changes')
    @mock.patch('subprocess.run')
    @mock.patch('checkout_helpers.check_btrfs')
    @mock.patch('checkout_helpers.get_gclient_root')
    def test_perform_chromium_setup_btrfs_force(self, mock_get_gclient_root,
                                                mock_check_btrfs,
                                                mock_subprocess_run,
                                                mock_check_uncommitted_changes,
                                                mock_build_chromium):
        """Tests setup with btrfs and force, skipping sudo -v."""
        mock_get_gclient_root.return_value = pathlib.Path('/root')
        mock_check_btrfs.return_value = True

        eval_prompts._perform_chromium_setup(force=True, build=True)

        mock_get_gclient_root.assert_called_once()
        mock_check_btrfs.assert_called_once_with(pathlib.Path('/root'))
        mock_subprocess_run.assert_not_called()
        mock_check_uncommitted_changes.assert_called_once_with(
            pathlib.Path('/root/src'))
        mock_build_chromium.assert_called_once_with(pathlib.Path('/root/src'))


class FetchSandboxImageUnittest(unittest.TestCase):
    """Unit tests for the `_fetch_sandbox_image` function."""

    def setUp(self):
        self.subprocess_run_patcher = mock.patch('subprocess.run')
        self.mock_subprocess_run = self.subprocess_run_patcher.start()
        self.addCleanup(self.subprocess_run_patcher.stop)

        self.get_gemini_version_patcher = mock.patch(
            'eval_prompts.gemini_helpers.get_gemini_version')
        self.mock_get_gemini_version = self.get_gemini_version_patcher.start()
        self.addCleanup(self.get_gemini_version_patcher.stop)

        self.mock_get_gemini_version.return_value = '1.2.3'

    def test_fetch_sandbox_image_success(self):
        """Tests that _fetch_sandbox_image returns true on success."""
        with self.assertLogs(level='INFO') as cm:
            result = eval_prompts._fetch_sandbox_image()
            self.assertTrue(result)
            self.assertIn('Pre-fetching sandbox image', cm.output[0])

        self.mock_subprocess_run.assert_called_once_with(
            [
                'docker', 'pull',
                'us-docker.pkg.dev/gemini-code-dev/gemini-cli/sandbox:1.2.3'
            ],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)

    def test_fetch_sandbox_image_get_version_fails(self):
        """Tests that _fetch_sandbox_image returns false on failure."""
        self.mock_get_gemini_version.return_value = None
        with self.assertLogs(level='ERROR') as cm:
            result = eval_prompts._fetch_sandbox_image()
            self.assertFalse(result)
            self.assertIn('Failed to get gemini version', cm.output[0])

    def test_fetch_sandbox_image_docker_pull_fails(self):
        """Tests that _fetch_sandbox_image returns false on failure."""
        error = subprocess.CalledProcessError(returncode=1, cmd='docker')
        error.stdout = 'mocked output'
        self.mock_subprocess_run.side_effect = error
        with self.assertLogs(level='ERROR') as cm:
            result = eval_prompts._fetch_sandbox_image()
            self.assertFalse(result)
            self.assertIn('Failed to pre-fetch sandbox image', cm.output[0])
            self.assertIn('mocked output', cm.output[0])


class RunPromptEvalTestsUnittest(unittest.TestCase):
    """Unit tests for the `_run_prompt_eval_tests` function."""

    def setUp(self):
        self._setUpMockArgs()
        self._setUpPatches()

    def _setUpMockArgs(self):
        """Set up mock arguments for the tests."""
        self.args = mock.Mock()
        self.args.shard_index = None
        self.args.total_shards = None
        self.args.filter = None
        self.args.force = False
        self.args.no_build = False
        self.args.promptfoo_revision = None
        self.args.promptfoo_version = None
        self.args.no_clean = False
        self.args.verbose = False
        self.args.sandbox = False
        self.args.print_output_on_success = False
        self.args.retries = 0
        self.args.parallel_workers = 1
        self.args.gemini_cli_bin = None
        self.args.promptfoo_bin = None
        self.args.isolated_script_test_repeat = 0

    def _setUpPatches(self):
        """Set up patches for the tests."""
        stdout_patcher = mock.patch('sys.stdout', new_callable=io.StringIO)
        self.mock_stdout = stdout_patcher.start()
        self.addCleanup(stdout_patcher.stop)

        worker_pool_patcher = mock.patch('eval_prompts.workers.WorkerPool')
        self.mock_worker_pool = worker_pool_patcher.start()
        self.addCleanup(worker_pool_patcher.stop)

        setup_promptfoo_patcher = mock.patch(
            'promptfoo_installation.setup_promptfoo')
        self.mock_setup_promptfoo = setup_promptfoo_patcher.start()
        self.addCleanup(setup_promptfoo_patcher.stop)

        perform_chromium_setup_patcher = mock.patch(
            'eval_prompts._perform_chromium_setup')
        self.mock_perform_chromium_setup = (
            perform_chromium_setup_patcher.start())
        self.addCleanup(perform_chromium_setup_patcher.stop)

        get_tests_to_run_patcher = mock.patch('eval_prompts._get_tests_to_run')
        self.mock_get_tests_to_run = get_tests_to_run_patcher.start()
        self.mock_get_tests_to_run.return_value = [
            pathlib.Path('/test/a.yaml')
        ]
        self.addCleanup(get_tests_to_run_patcher.stop)

        subprocess_run_patcher = mock.patch('subprocess.run')
        self.mock_subprocess_run = subprocess_run_patcher.start()
        self.addCleanup(subprocess_run_patcher.stop)

        fetch_sandbox_image_patcher = mock.patch(
            'eval_prompts._fetch_sandbox_image')
        self.mock_fetch_sandbox_image = fetch_sandbox_image_patcher.start()
        self.addCleanup(fetch_sandbox_image_patcher.stop)

    def test_run_prompt_eval_tests_no_tests(self):
        """Tests that the function returns 1 if there are no tests to run."""
        self.mock_get_tests_to_run.return_value = []
        returncode = eval_prompts._run_prompt_eval_tests(self.args)
        self.assertEqual(returncode, 1)

    def test_run_prompt_eval_tests_one_test_pass(self):
        """Tests running a single passing test."""
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = []
        with self.assertLogs(level='INFO') as cm:
            returncode = eval_prompts._run_prompt_eval_tests(self.args)
            self.assertIn('Successfully ran 1 tests', cm.output[-1])

        self.mock_perform_chromium_setup.assert_called_once_with(force=False,
                                                                 build=True)
        self.mock_setup_promptfoo.assert_called_once()
        self.mock_worker_pool.assert_called_once()
        self.mock_worker_pool.return_value.queue_tests.assert_called_once_with(
            [pathlib.Path('/test/a.yaml')])
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            assert_called_once()
        self.mock_worker_pool.return_value.shutdown_blocking.assert_called_once(
        )
        self.assertEqual(returncode, 0)

    def test_run_prompt_eval_tests_one_test_fail(self):
        """Tests running a single failing test."""
        failed_test = results.TestResult(test_file='test',
                                         success=False,
                                         duration=1,
                                         test_log='',
                                         metrics={})
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = [
                failed_test
            ]

        self.args.no_build = True
        self.args.no_clean = True
        self.args.verbose = True
        with self.assertLogs(level='WARNING') as cm:
            returncode = eval_prompts._run_prompt_eval_tests(self.args)
            self.assertIn(
                '0 tests ran successfully and 1 failed after 0 additional '
                'tries', cm.output[-3])
            self.assertIn('Failed tests:', cm.output[-2])
            self.assertIn('  test', cm.output[-1])

        self.mock_perform_chromium_setup.assert_called_once_with(force=False,
                                                                 build=False)
        self.assertEqual(returncode, 1)

    def test_run_prompt_eval_tests_multiple_tests_one_fail(self):
        """Tests running multiple tests where one fails."""
        self.mock_get_tests_to_run.return_value = [
            pathlib.Path('/test/a.yaml'),
            pathlib.Path('/test/b.yaml'),
            pathlib.Path('/test/c.yaml'),
        ]
        failed_test = results.TestResult(test_file='test',
                                         success=False,
                                         duration=1,
                                         test_log='',
                                         metrics={})
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = [
                failed_test
            ]

        with self.assertLogs(level='WARNING') as cm:
            returncode = eval_prompts._run_prompt_eval_tests(self.args)
            self.assertIn(
                '2 tests ran successfully and 1 failed after 0 additional '
                'tries', cm.output[-3])
            self.assertIn('Failed tests:', cm.output[-2])
            self.assertIn('  test', cm.output[-1])

        self.mock_perform_chromium_setup.assert_called_once_with(force=False,
                                                                 build=True)
        self.assertEqual(returncode, 1)

    def test_run_prompt_eval_tests_sandbox_prefetch_fails(self):
        """Tests that _run_prompt_eval_tests exits and logs output if sandbox
        pre-fetch fails."""
        self.args.sandbox = True
        self.mock_fetch_sandbox_image.return_value = False
        result = eval_prompts._run_prompt_eval_tests(self.args)
        self.assertEqual(result, 1)

    def test_run_prompt_eval_tests_with_sandbox_enabled(self):
        """Tests that _run_prompt_eval_tests calls pre-fetch and passes sandbox
        var when enabled."""
        self.args.sandbox = True
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = []
        self.mock_fetch_sandbox_image.return_value = True

        eval_prompts._run_prompt_eval_tests(self.args)

        self.mock_fetch_sandbox_image.assert_called_once()
        self.mock_worker_pool.assert_called_once()
        self.assertTrue(self.mock_worker_pool.call_args[0][2].sandbox)

    def test_run_prompt_eval_tests_with_sandbox_disabled(self):
        """Tests that _run_prompt_eval_tests does not call pre-fetch or pass
        sandbox var when disabled."""
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = []
        eval_prompts._run_prompt_eval_tests(self.args)

        self.mock_subprocess_run.assert_not_called()
        self.mock_worker_pool.assert_called_once()
        self.assertFalse(self.mock_worker_pool.call_args[0][2].sandbox)

    def test_run_prompt_eval_tests_retry_pass(self):
        """Tests that a test that passes on retry is recorded as a success."""
        self.args.retries = 1
        failed_test = results.TestResult(test_file='test',
                                         success=False,
                                         duration=1,
                                         test_log='',
                                         metrics={})
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            side_effect = [
                [failed_test],
                [],
            ]

        with self.assertLogs(level='INFO') as cm:
            returncode = eval_prompts._run_prompt_eval_tests(self.args)
            self.assertIn('Successfully ran 1 tests', cm.output[-1])

        self.assertEqual(
            self.mock_worker_pool.return_value.queue_tests.call_count, 2)
        self.assertEqual(returncode, 0)

    def test_run_prompt_eval_tests_retry_fail(self):
        """Tests that a test that fails all retries is recorded as a fail."""
        self.args.retries = 2
        failed_test = results.TestResult(test_file='test',
                                         success=False,
                                         duration=1,
                                         test_log='',
                                         metrics={})
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = [
                failed_test
            ]

        with self.assertLogs(level='WARNING') as cm:
            returncode = eval_prompts._run_prompt_eval_tests(self.args)
            self.assertIn(
                '0 tests ran successfully and 1 failed after 2 additional '
                'tries', cm.output[-3])

        self.assertEqual(
            self.mock_worker_pool.return_value.queue_tests.call_count, 3)
        self.assertEqual(returncode, 1)

    def test_run_prompt_eval_tests_no_retry_on_pass(self):
        """Tests that a passing test is not retried."""
        self.args.retries = 5
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = []

        with self.assertLogs(level='INFO') as cm:
            returncode = eval_prompts._run_prompt_eval_tests(self.args)
            self.assertIn('Successfully ran 1 tests', cm.output[-1])

        self.assertEqual(
            self.mock_worker_pool.return_value.queue_tests.call_count, 1)
        self.assertEqual(returncode, 0)

    def test_run_prompt_eval_tests_with_custom_bins(self):
        """Tests that custom binaries are used when provided."""
        self.args.promptfoo_bin = pathlib.Path('/custom/promptfoo')
        self.args.gemini_cli_bin = pathlib.Path('/custom/gemini')
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = []

        with mock.patch(
                'promptfoo_installation.PreinstalledPromptfooInstallation'
        ) as mock_preinstalled:
            eval_prompts._run_prompt_eval_tests(self.args)
            mock_preinstalled.assert_called_once_with(
                pathlib.Path('/custom/promptfoo'))

        self.mock_setup_promptfoo.assert_not_called()
        self.mock_worker_pool.assert_called_once()
        self.assertEqual(self.mock_worker_pool.call_args[0][2].gemini_cli_bin,
                         pathlib.Path('/custom/gemini'))

    def test_run_prompt_eval_tests_with_repeat(self):
        """Tests that tests are repeated correctly."""
        self.args.isolated_script_test_repeat = 3
        self.mock_get_tests_to_run.return_value = [
            pathlib.Path('/test/a.yaml')
        ]
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = []

        with self.assertLogs(level='INFO') as cm:
            returncode = eval_prompts._run_prompt_eval_tests(self.args)
            self.assertIn('Successfully ran 4 tests', cm.output[-1])

        self.mock_worker_pool.return_value.queue_tests.assert_called_once_with(
            [pathlib.Path('/test/a.yaml')] * 4)
        self.assertEqual(returncode, 0)

    def test_run_prompt_eval_tests_full_parallel(self):
        """Tests that a -1 parallel workers makes a worker for each test."""
        self.mock_get_tests_to_run.return_value = [
            pathlib.Path('/test/a.yaml'),
            pathlib.Path('/test/b.yaml'),
            pathlib.Path('/test/c.yaml'),
        ]
        self.mock_worker_pool.return_value.wait_for_all_queued_tests.\
            return_value = []
        self.args.parallel_workers = -1

        returncode = eval_prompts._run_prompt_eval_tests(self.args)
        self.mock_worker_pool.assert_called_with(3, mock.ANY, mock.ANY,
                                                 mock.ANY)
        self.assertEqual(returncode, 0)


class ParseArgsUnittest(unittest.TestCase):
    """Unit tests for the `_parse_args` function."""

    def setUp(self):
        """Set up patches for the tests."""
        argv_patcher = mock.patch('sys.argv', new_callable=list)
        self.mock_argv = argv_patcher.start()
        self.addCleanup(argv_patcher.stop)

    def test_parse_args_no_args(self):
        """Tests that default values are correct with no arguments."""
        self.mock_argv[:] = ['eval_prompts.py']
        args = eval_prompts._parse_args()
        self.assertFalse(args.no_clean)
        self.assertFalse(args.force)
        self.assertFalse(args.no_build)
        self.assertFalse(args.verbose)
        self.assertFalse(args.print_output_on_success)
        self.assertIsNone(args.isolated_script_test_output)
        self.assertIsNone(args.isolated_script_test_perf_output)
        self.assertIsNone(args.filter)
        self.assertIsNone(args.shard_index)
        self.assertIsNone(args.total_shards)
        self.assertIsNone(args.promptfoo_bin)
        self.assertIsNone(args.promptfoo_version)
        self.assertIsNone(args.promptfoo_revision)
        self.assertFalse(args.sandbox)
        self.assertIsNone(args.gemini_cli_bin)
        self.assertEqual(args.parallel_workers, 1)
        self.assertEqual(args.retries, 0)
        self.assertEqual(args.isolated_script_test_repeat, 0)

    def test_parse_args_all_checkout_args(self):
        """Tests that all checkout arguments are parsed correctly."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--no-clean', '--force', '--no-build'
        ]
        args = eval_prompts._parse_args()
        self.assertTrue(args.no_clean)
        self.assertTrue(args.force)
        self.assertTrue(args.no_build)

    def test_parse_args_all_output_args(self):
        """Tests that all output arguments are parsed correctly."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--verbose', '--print-output-on-success'
        ]
        args = eval_prompts._parse_args()
        self.assertTrue(args.verbose)
        self.assertTrue(args.print_output_on_success)

    def test_parse_args_all_test_selection_args(self):
        """Tests that all test selection arguments are parsed correctly."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--filter', 'my_filter', '--shard-index', '1',
            '--total-shards', '3'
        ]
        args = eval_prompts._parse_args()
        self.assertEqual(args.filter, 'my_filter')
        self.assertEqual(args.shard_index, 1)
        self.assertEqual(args.total_shards, 3)

    def test_parse_args_isolated_script_test_filter(self):
        """Tests the --isolated-script-test-filter argument."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--isolated-script-test-filter', 'iso_filter'
        ]
        args = eval_prompts._parse_args()
        self.assertEqual(args.filter, 'iso_filter')

    def test_parse_args_filter_exclusive_group(self):
        """Tests that filter arguments are mutually exclusive."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--filter', 'a',
            '--isolated-script-test-filter', 'b'
        ]
        # stderr mocked to silence the automatic help output by the parser when
        # parsing fails.
        with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
            eval_prompts._parse_args()

    def test_parse_args_all_gemini_cli_args(self):
        """Tests that all gemini-cli arguments are parsed correctly."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--sandbox', '--gemini-cli-bin',
            '/path/to/gemini'
        ]
        args = eval_prompts._parse_args()
        self.assertTrue(args.sandbox)
        self.assertEqual(args.gemini_cli_bin, pathlib.Path('/path/to/gemini'))

    def test_parse_args_all_test_runner_args(self):
        """Tests that all test runner arguments are parsed correctly."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--parallel-workers', '4', '--retries', '2',
            '--isolated-script-test-repeat', '3'
        ]
        args = eval_prompts._parse_args()
        self.assertEqual(args.parallel_workers, 4)
        self.assertEqual(args.retries, 2)
        self.assertEqual(args.isolated_script_test_repeat, 3)

    def test_parse_args_full_parallel_args(self):
        """Tests that all test runner arguments are parsed correctly."""
        self.mock_argv[:] = ['eval_prompts.py', '--parallel-workers', '-1']
        args = eval_prompts._parse_args()
        self.assertEqual(args.parallel_workers, -1)

    def test_parse_args_isolated_script_test_launcher_retry_limit(self):
        """Tests the --isolated-script-test-launcher-retry-limit argument."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--isolated-script-test-launcher-retry-limit',
            '3'
        ]
        args = eval_prompts._parse_args()
        self.assertEqual(args.retries, 3)

    def test_parse_args_retries_exclusive_group(self):
        """Tests that retry arguments are mutually exclusive."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--retries', '2',
            '--isolated-script-test-launcher-retry-limit', '3'
        ]
        # stderr mocked to silence the automatic help output by the parser when
        # parsing fails.
        with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
            eval_prompts._parse_args()

    def test_parse_args_promptfoo_bin(self):
        """Tests --promptfoo-bin."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--promptfoo-bin', '/path/to/promptfoo'
        ]
        args = eval_prompts._parse_args()
        self.assertEqual(args.promptfoo_bin,
                         pathlib.Path('/path/to/promptfoo'))

    def test_parse_args_promptfoo_version(self):
        """Tests --install-promptfoo-from-npm with a version."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--install-promptfoo-from-npm', '0.40.0'
        ]
        args = eval_prompts._parse_args()
        self.assertEqual(args.promptfoo_version, '0.40.0')

    def test_parse_args_promptfoo_revision(self):
        """Tests --install-promptfoo-from-src with a revision."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--install-promptfoo-from-src', 'my-rev'
        ]
        args = eval_prompts._parse_args()
        self.assertEqual(args.promptfoo_revision, 'my-rev')

    def test_parse_args_promptfoo_exclusive_group(self):
        """Tests that mutually exclusive promptfoo arguments raise an error."""
        arg_groups = [
            ['--promptfoo-bin', '/path/to/promptfoo'],
            ['--install-promptfoo-from-npm'],
            ['--install-promptfoo-from-src'],
        ]
        for arg_group1, arg_group2 in itertools.combinations(arg_groups, 2):
            with self.subTest(args1=arg_group1, args2=arg_group2):
                self.mock_argv[:] = (['eval_prompts.py'] + arg_group1 +
                                     arg_group2)
                # stderr mocked to silence the automatic help output by the
                # parser when parsing fails.
                with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
                    eval_prompts._parse_args()

    def test_parse_args_negative_shard_index(self):
        """Tests that a negative shard_index raises an error."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--shard-index', '-1', '--total-shards', '2'
        ]
        with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
            eval_prompts._parse_args()

    def test_parse_args_zero_total_shards(self):
        """Tests that a total_shards of zero raises an error."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--shard-index', '0', '--total-shards', '0'
        ]
        with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
            eval_prompts._parse_args()

    def test_parse_args_shard_index_only(self):
        """Tests that providing only shard_index raises an error."""
        self.mock_argv[:] = ['eval_prompts.py', '--shard-index', '1']
        with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
            eval_prompts._parse_args()

    def test_parse_args_total_shards_only(self):
        """Tests that providing only total_shards raises an error."""
        self.mock_argv[:] = ['eval_prompts.py', '--total-shards', '2']
        with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
            eval_prompts._parse_args()

    def test_parse_args_zero_parallel_workers(self):
        """Tests that zero parallel_workers raises an error."""
        self.mock_argv[:] = ['eval_prompts.py', '--parallel-workers', '0']
        with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
            eval_prompts._parse_args()

    def test_parse_args_negative_retries(self):
        """Tests that negative retries raises an error."""
        self.mock_argv[:] = ['eval_prompts.py', '--retries', '-1']
        with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
            eval_prompts._parse_args()

    def test_parse_args_negative_repeat(self):
        """Tests that negative repeat raises an error."""
        self.mock_argv[:] = [
            'eval_prompts.py', '--isolated-script-test-repeat', '-1'
        ]
        with self.assertRaises(SystemExit), mock.patch('sys.stderr'):
            eval_prompts._parse_args()



if __name__ == '__main__':
    unittest.main()
