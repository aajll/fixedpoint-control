project('fixedpoint-control', 'c', version: '1.2.2', default_options: ['c_std=c11'])

conf = configuration_data()
conf.set('VERSION', meson.project_version())
version_header = configure_file(
  input: 'config/fpc_version.h.in',
  output: 'fpc_version.h',
  configuration: conf,
  install: false,
)

fpc_build_conf = configuration_data()
fpc_max_instances = get_option('fpc_max_instances')
fpc_pool_item_size = get_option('fpc_pool_item_size')
fpc_build_conf.set('FPC_MAX_INSTANCES', fpc_max_instances)
fpc_build_conf.set('FPC_POOL_ITEM_SIZE', fpc_pool_item_size)
conf_header = configure_file(
  input: 'config/fpc_conf.h.in',
  output: 'fpc_conf.h',
  configuration: fpc_build_conf,
  install: false,
)

inc = include_directories('include', '.')

# Pool allocator subproject
# Note: pool_item_size must be a multiple of max_align_t (16 on this system)
pool_subproj = subproject(
  'pool-allocator',
  default_options: [
    'build_tests=false',
    'lookup_strategy=linear',
    'pool_max_slots=@0@'.format(fpc_max_instances),
    'pool_item_size=@0@'.format(fpc_pool_item_size),
  ],
)
pool_lib = pool_subproj.get_variable('pool_lib')
pool_dep = declare_dependency(include_directories: pool_subproj.get_variable('inc'))

subdir('src')

fpc_dep = declare_dependency(link_with: lib, include_directories: inc)

meson.override_dependency('fixedpoint-control', fpc_dep)

install_headers(
  files(
    'include/fpc_config.h',
    'include/fpc_filters.h',
    'include/fpc_pid.h',
    'include/fpc_status.h',
  ),
  version_header,
  conf_header,
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
