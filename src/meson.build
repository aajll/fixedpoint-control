src_files = files('pid_controller.c', 'filters.c')

# Build as static library for testing
lib = static_library('fpc',
        src_files,
        include_directories : inc,
        dependencies : [pool_dep, fxp_dep],
        link_whole : pool_lib,
        install : true)
