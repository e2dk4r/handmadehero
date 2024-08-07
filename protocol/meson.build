fs = import('fs')

wayland_protocols = dependency('wayland-protocols', native: true)
wl_protocol_dir = wayland_protocols.get_variable('pkgdatadir')

wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
  wayland_scanner_dep.get_variable('wayland_scanner'),
  native: true,
)

protocols = [
  wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
  wl_protocol_dir / 'stable/viewporter/viewporter.xml',
  wl_protocol_dir / 'stable/presentation-time/presentation-time.xml',
  wl_protocol_dir / 'staging/content-type/content-type-v1.xml',
  # wl_protocol_dir / 'unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml',
]

wl_protocols_src = []

foreach xml : protocols
  if not fs.exists(xml)
    xml = fs.name(xml)
  endif

  wl_protocols_src += custom_target(
    xml.underscorify() + '_c',
    input: xml,
    output: '@BASENAME@-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  wl_protocols_src += custom_target(
    xml.underscorify() + '_client_h',
    input: xml,
    output: '@BASENAME@-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
endforeach
