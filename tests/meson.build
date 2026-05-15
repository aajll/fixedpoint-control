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
endif
