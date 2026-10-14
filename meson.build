project('fixedpoint-control', 'c', version: '0.3.2', default_options: ['c_std=c11', 'warning_level=3'])

conf = configuration_data()
conf.set('VERSION', meson.project_version())
version_header = configure_file(
  input: 'config/fpc_version.h.in',
  output: 'fpc_version.h',
  configuration: conf,
  install: false,
)

inc = include_directories('include', '.')

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
  ),
  version_header,
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
