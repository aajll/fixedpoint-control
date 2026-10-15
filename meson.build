# SPDX-License-Identifier: MIT

project(
  'fixedpoint-control',
  'c',
  version: '0.3.2',
  meson_version: '>=0.57.0',
  default_options: ['c_std=c11', 'warning_level=3'],
)

# include/fpc_status.h defines the version macros, so a copy-in build has them.
# Configuration fails when they differ from the project version.

fs = import('fs')
version_parts = meson.project_version().split('.')
version_macros = {
  'FPC_VERSION_MAJOR': version_parts[0],
  'FPC_VERSION_MINOR': version_parts[1],
  'FPC_VERSION_PATCH': version_parts[2],
  'FPC_VERSION': '"' + meson.project_version() + '"',
}
header_lines = []
foreach line : fs.read('include' / 'fpc_status.h').split('\n')
  header_lines += ' '.join(line.split())
endforeach
foreach name, value : version_macros
  if '#define @0@ @1@'.format(name, value) not in header_lines
    error('include/fpc_status.h must define @0@ as @1@'.format(name, value))
  endif
endforeach

inc = include_directories('include')

# Fixed-point arithmetic kernel. Its public header is part of the fpc API, so
# fpc_dep carries it to consumers.
fxp_dep = dependency(
  'fxp',
  default_options: ['build_tests=false', 'with_convert=false'],
)

subdir('src')

fpc_dep = declare_dependency(
  link_with: lib,
  include_directories: inc,
  dependencies: fxp_dep,
)

meson.override_dependency('fixedpoint-control', fpc_dep)

install_headers(
  files(
    'include/fpc_filters.h',
    'include/fpc_pid.h',
    'include/fpc_status.h',
    'include/fpc_version.h',
  ),
  subdir: 'fixedpoint-control',
)

if get_option('build_tests')
  subdir('tests')
endif

pkg = import('pkgconfig')
pkg.generate(
  lib,
  name: 'fixedpoint-control',
  filebase: 'fixedpoint-control',
  description: 'Safety-oriented fixed-point PID and filter library',
  subdirs: 'fixedpoint-control',
)
