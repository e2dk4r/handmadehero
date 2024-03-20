project(
  'handmadehero',
  'c',
  version: '0.1-dev',
  default_options: [
    'c_std=c99',
    'warning_level=2',
    'werror=true',
  ],
)

is_build_debug = get_option('buildtype').startswith('debug')
cc = meson.get_compiler('c')

add_project_arguments(
  [
    '-march=x86-64-v3',
    '-funroll-loops',
    '-fomit-frame-pointer',

    '-Wconversion',
    '-Wno-unused-parameter',
    '-Wno-unused-result',
    '-Wno-missing-braces',
  ],
  language: 'c',
)

wayland_client = dependency('wayland-client')
wayland_protocols = dependency('wayland-protocols', native: true)
xkbcommon = dependency('xkbcommon')
libevdev = dependency('libevdev')
liburing = dependency('liburing')
vulkan = dependency('vulkan')
libm = cc.find_library('m')

subdir('protocol')

common_src = [
  'src/handmadehero_memory_arena.c'
]

platform_src = [
  'src/handmadehero_linux.c',
]

if is_build_debug
  add_project_arguments(
    [
      '-DHANDMADEHERO_DEBUG=1',
      '-DHANDMADEHERO_INTERNAL=1',

      '-Wno-unused-but-set-variable',
      '-Wno-unused-function',
      '-Wno-unused-variable',
    ],
    language : 'c'
  )
  platform_src += 'src/debug.c'
endif

# debug builds have handmadehero as dynamic library
# and does not need to link with because of hot reload logic
handmadehero_sources = [
  'src/handmadehero_tile.c',
  'src/handmadehero.c',
]

if is_build_debug
  handmadeheroLib = library(
    'handmadehero',
    sources: common_src + handmadehero_sources + [
      'src/debug.c',
    ],
    include_directories: 'include',
    dependencies: [
      libm,
    ]
  )
else
  platform_src += handmadehero_sources
endif

subdir('tool')

executable(
  'handmadehero',
  sources: common_src + platform_src + wl_protocols_src,
  include_directories: [
    'include',
    'protocol',
  ],
  dependencies: [
    wayland_client,
    xkbcommon,
    libevdev,
    liburing,
    vulkan,
    libm,
  ]
)
