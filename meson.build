project(
  'handmadehero',
  'c',
  'nasm',
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
    '-DCOMPILER_GCC=' + (cc.get_id() == 'gcc').to_int().to_string(),
    '-DCOMPILER_CLANG=' + (cc.get_id() == 'clang').to_int().to_string(),
  ],
  language: 'c',
)

add_project_arguments(
  cc.get_supported_arguments([
    '-O3',
    '-march=x86-64-v3',
    '-funroll-loops',
    '-fomit-frame-pointer',

    '-Wconversion',
    '-Wno-unused-parameter',
    '-Wno-unused-result',
    '-Wno-missing-braces',
  ]),
  language: 'c',
)

if is_build_debug
  add_project_arguments(
    cc.get_supported_arguments([
      '-DHANDMADEHERO_DEBUG=1',
      '-DHANDMADEHERO_INTERNAL=1',

      '-Wno-unused-but-set-variable',
      '-Wno-unused-function',
      '-Wno-unused-variable',
    ]),
    language : 'c'
  )
endif

libm = cc.find_library('m')

# used in both library and platform code
common_src = [
  'src/handmadehero_memory_arena.c'
]
if is_build_debug
  common_src += [
    'src/debug.c',
    'src/rdtsc.asm',
  ]
endif

# debug builds have handmadehero as dynamic library
# and does not need to link with because of hot reload logic
handmadehero_sources = [
  'src/random.c',
  'src/handmadehero_asset.c',
  'src/handmadehero_audio.c',
  'src/handmadehero_entity.c',
  'src/handmadehero_render_group.c',
  'src/handmadehero_sim_region.c',
  'src/handmadehero_world.c',
  'src/handmadehero.c',
]

if is_build_debug
  handmadeheroLib = library(
    'handmadehero',
    sources: common_src + handmadehero_sources,
    include_directories: 'include',
    dependencies: [
      libm,
    ]
  )
else
  common_src += handmadehero_sources
endif

platform_src = []
platform_inc_dirs = []
platform_libs = []
if host_machine.system() == 'windows'
  #################################################################
  # TODO: publish on windows platform
  # WINDOWS BUILD
  #        _.-;;-._
  # '-..-'|   ||   |
  # '-..-'|_.-;;-._|
  # '-..-'|   ||   |
  # '-..-'|_.-''-._|
  #################################################################

elif host_machine.system() == 'linux'
  #################################################################
  # LINUX BUILD
  #      .--.
  #     |o_o |
  #     |:_/ |
  #    //   \ \
  #   (|     | )
  #  /'\_   _/`\
  #  \___)=(___/
  #################################################################
  wayland_client = dependency('wayland-client')
  wayland_protocols = dependency('wayland-protocols', native: true)
  xkbcommon = dependency('xkbcommon')
  libevdev = dependency('libevdev')
  liburing = dependency('liburing')
  libpthread = cc.find_library('pthread')
  libpipewire = dependency('libpipewire-0.3')
  subdir('protocol')

  platform_src += [
    'src/handmadehero_linux.c',
  ]
  platform_src += wl_protocols_src
  platform_inc_dirs += [
    'protocol',
  ]
  platform_libs += [
    wayland_client,
    xkbcommon,
    libevdev,
    liburing,
    libm,
    libpthread,
    libpipewire,
  ]
endif

subdir('tool')
subdir('test')

executable(
  'handmadehero',
  sources: common_src + platform_src,
  include_directories: [
    'include',
    platform_inc_dirs,
  ],
  dependencies: platform_libs,
)
