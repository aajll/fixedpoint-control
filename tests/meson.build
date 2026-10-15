# SPDX-License-Identifier: MIT

if get_option('build_tests')
  test_exe = executable(
    'test_fpc',
    [
      'test_main.c',
      'test_harness.c',
      'test_pid.c',
      'test_filters.c',
    ],
    dependencies: [fpc_dep],
    include_directories: inc,
  )

  test('fixedpoint-control unit tests', test_exe, protocol: 'tap')

  # Compiles every C block in the README, CONTRIBUTING.md and docs/. The
  # examples include fxp.h through the public headers, so the test runs when
  # fxp is built as a subproject and its headers are in subprojects/fxp/.
  if fxp_dep.type_name() == 'internal'
    python = import('python').find_installation('python3')
    test(
      'fixedpoint-control docs examples',
      python,
      args: [
        files('docs/check_snippets.py'),
        '--root', meson.project_source_root(),
        '--lang', 'c',
        '--',
      ] + meson.get_compiler('c').cmd_array() + [
        '-std=c11',
        '-Wall',
        '-Wextra',
        '-Wpedantic',
        '-Werror',
        '-Wno-unused-function',
        '-Wno-unused-variable',
        '-I' + (meson.project_source_root() / 'include'),
        '-I' + (meson.project_source_root() / 'subprojects' / 'fxp' / 'include'),
        '-c', '{source}', '-o', '{object}',
      ],
      suite: 'docs',
      timeout: 60,
    )
  endif
endif
