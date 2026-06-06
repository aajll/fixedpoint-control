project('fixedpoint-control', 'c', version: '1.3.2', default_options: ['c_std=c11', 'warning_level=3'])

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
fpc_filter_max_order = get_option('fpc_filter_max_order')

# Derive the pool slot size from the largest user-visible struct,
# `struct fpc_fir` ≈ 8 bytes of header + 2 * int32_t * order. Round up to
# 16 (a safe upper bound for max_align_t on common targets). The C-side
# `_Static_assert` is the authoritative check; this formula just keeps the
# Meson and C views in lock-step so the assert can never fire under default
# fpc usage.
fpc_pool_item_size = (((8 + (8 * fpc_filter_max_order)) + 15) / 16) * 16

fpc_build_conf.set('FPC_MAX_INSTANCES', fpc_max_instances)
fpc_build_conf.set('FPC_FILTER_MAX_ORDER', fpc_filter_max_order)
conf_header = configure_file(
  input: 'config/fpc_conf.h.in',
  output: 'fpc_conf.h',
  configuration: fpc_build_conf,
  install: false,
)

inc = include_directories('include', '.')

# Pool allocator subproject. fpc owns the capacity (slots) and the slot size
# (derived above); atomicity_mode is left at the subproject's `auto` default,
# which picks `volatile` on TI C2000 and `_Atomic` on hosted targets.
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
pool_dep = declare_dependency(include_directories: pool_subproj.get_variable('public_headers'))

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
