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
libudev = dependency('libudev')
liburing = dependency('liburing')
vulkan = dependency('vulkan')

subdir('protocol')

sources = [
  'src/handmadehero_linux.c',
]

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DHANDMADEHERO_DEBUG=1', language : 'c')
  add_project_arguments('-DHANDMADEHERO_INTERNAL=1', language : 'c')
  sources += 'src/debug.c'
endif

# debug builds have handmadehero as dynamic library
if get_option('buildtype').startswith('debug')
  handmadeheroLib = library('handmadehero', sources: ['src/handmadehero.c', 'src/debug.c',], include_directories: 'include')
else
  sources += 'src/handmadehero.c'
endif

subdir('tool')

executable(
	'handmadehero',
	sources: sources + wl_protocols_src,
	include_directories: [
		'include',
		'protocol',
	],
	dependencies: [
		wayland_client,
		xkbcommon,
		libudev,
		liburing,
		vulkan,
	]
)
