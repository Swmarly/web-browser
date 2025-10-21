#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('cmd', choices=['desc'])
  parser.add_argument('--root', required=True)
  parser.add_argument('outdir')
  parser.add_argument('gn_target', choices=['//foo'])
  parser.add_argument('what', choices=['deps'])
  parser.add_argument('--as', choices=['buildfile'], required=True)
  parser.add_argument('--all', required=True, action='store_true')
  args = parser.parse_args()

  print(f'{args.root}/third_party/sample3/BUILD.gn')
  print(f'{args.root}/third_party/sample2/BUILD.gn')
  print(f'{args.root}/third_party/pruned/BUILD.gn')
  print(f'{args.root}/some/other/dir/BUILD.gn')
  return 0


if __name__ == '__main__':
  sys.exit(main())
