#!/usr/bin/env python

# Copyright 2016 WebAssembly Community Group participants
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
from support import run_command
from shared import (
    fail, fail_with_error, fail_if_not_contained,
    options, S2WASM, WASM_SHELL
)


def test_s2wasm():
  print '\n[ checking .s testcases... ]\n'

  cmd = S2WASM + [
      os.path.join(options.binaryen_test, 'dot_s', 'basics.s'),
      '--import-memory']
  output = run_command(cmd)
  fail_if_not_contained(
      output, '(import "env" "memory" (memory $0 1))')

  for dot_s_dir in ['dot_s', 'llvm_autogenerated']:
    dot_s_path = os.path.join(options.binaryen_test, dot_s_dir)
    for s in sorted(os.listdir(dot_s_path)):
      if not s.endswith('.s'):
        continue
      print '..', s
      wasm = s.replace('.s', '.wast')
      full = os.path.join(options.binaryen_test, dot_s_dir, s)
      stack_alloc = (['--allocate-stack=1024']
                     if dot_s_dir == 'llvm_autogenerated'
                     else [])
      cmd = S2WASM + [full, '--emscripten-glue'] + stack_alloc
      if s.startswith('start_'):
        cmd.append('--start')
      actual = run_command(cmd)

      # verify output
      expected_file = os.path.join(options.binaryen_test, dot_s_dir, wasm)
      if not os.path.exists(expected_file):
        print actual
        fail_with_error('output ' + expected_file + ' does not exist')
      expected = open(expected_file, 'rb').read()
      if actual != expected:
        fail(actual, expected)

      # verify with options
      cmd = S2WASM + [full, '--global-base=1024'] + stack_alloc
      run_command(cmd)

      # run wasm-shell on the .wast to verify that it parses
      cmd = WASM_SHELL + [expected_file]
      run_command(cmd)


def test_linker():
  print '\n[ running linker tests... ]\n'
  # The {main,foo,bar,baz}.s files were created by running clang over the
  # respective c files. The foobar.bar archive was created by running:
  # llvm-ar -format=gnu rc foobar.a quux.s foo.s bar.s baz.s
  cmd = S2WASM + [
      os.path.join(options.binaryen_test, 'linker', 'main.s'), '-l',
      os.path.join(options.binaryen_test, 'linker', 'archive', 'foobar.a')]
  output = run_command(cmd)
  # foo should come from main.s and return 42
  fail_if_not_contained(output, '(func $foo')
  fail_if_not_contained(output, '(i32.const 42)')
  # bar should be linked in from bar.s
  fail_if_not_contained(output, '(func $bar')
  # quux should be linked in from bar.s even though it comes before bar.s in
  # the archive
  fail_if_not_contained(output, '(func $quux')
  # baz should not be linked in at all
  if 'baz' in output:
    fail_with_error('output should not contain "baz": ' + output)

  # Test an archive using a string table
  cmd = S2WASM + [
      os.path.join(options.binaryen_test, 'linker', 'main.s'), '-l',
      os.path.join(options.binaryen_test, 'linker', 'archive', 'barlong.a')]
  output = run_command(cmd)
  # bar should be linked from the archive
  fail_if_not_contained(output, '(func $bar')

  # Test exporting memory growth function and emscripten runtime functions
  cmd = S2WASM + [
      os.path.join(options.binaryen_test, 'linker', 'main.s'),
      '--emscripten-glue', '--allow-memory-growth']
  output = run_command(cmd)
  expected_funcs = [
      ('__growWasmMemory', '(param $newSize i32)'),
      ('stackSave', '(result i32)'),
      ('stackAlloc', '(param $0 i32) (result i32)'),
      ('stackRestore', '(param $0 i32)'),
  ]
  for name, extra in expected_funcs:
    space = ' ' if extra else ''
    fail_if_not_contained(output, '(export "{0}" (func ${0}))'.format(name))
    fail_if_not_contained(output, '(func ${0}'.format(name + space + extra))


if __name__ == '__main__':
  test_s2wasm()
  test_linker()
