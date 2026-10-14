src_files = files('pid_controller.c', 'filters.c')

# Build as static library for testing
lib = static_library('fpc',
        src_files,
        include_directories : inc,
        dependencies : fxp_dep,
        install : true)
