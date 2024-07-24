if get_option('hh_record_read')
  executable(
    'hh_record_read',
    sources: [
      'hh_record_read/main.c'
    ],
    include_directories: [
      '../include',
    ],
  )
endif

assert(get_option('truetype_backend').length() == 1, 'You must select one truetype backend.')
truetype_backend = get_option('truetype_backend')[0]
libfreetype = dependency('freetype2', required: truetype_backend == 'freetype')

if get_option('hh_asset_builder')
  executable(
    'hh_asset_builder',
    sources: [
      'hh_asset_builder/main.c'
    ],
    include_directories: [
      '../include',
    ],
    c_args: [
      '-DTRUETYPE_BACKEND_FREETYPE=' + (truetype_backend == 'freetype').to_int().to_string(),
      '-DTRUETYPE_BACKEND_STBTT=' + (truetype_backend == 'stbtt').to_int().to_string(),
    ],
    # link_args: '-static',
    dependencies: [
      libm,
      libfreetype,
    ]
  )
endif
