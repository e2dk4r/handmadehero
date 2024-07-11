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

if get_option('hh_asset_builder')
  executable(
    'hh_asset_builder',
    sources: [
      'hh_asset_builder/main.c'
    ],
    include_directories: [
      '../include',
    ],
    link_args: '-static',
    dependencies: [
      libm
    ]
  )
endif
