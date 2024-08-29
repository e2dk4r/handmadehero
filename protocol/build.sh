# vi: set et ft=sh ts=2 sw=2 fenc=utf-8 :vi
WaylandProtocolsDir=$(pkg-config --variable=pkgdatadir wayland-protocols)
WaylandProtocolsDir=${WaylandProtocolsDir%/}
if [ -z "$WaylandProtocolsDir" ]; then
  Log "could not find wayland-protocols!"
  exit 1
fi

outputDir="$OutputDir/protocol"
if [ ! -e "$outputDir" ]; then
  mkdir "$outputDir"
fi

StartTimer

WaylandProtocolsInc="-I$outputDir"
WaylandProtocolsSrc=''
for protocol in 'stable/xdg-shell/xdg-shell.xml' \
                'stable/viewporter/viewporter.xml' \
                'stable/presentation-time/presentation-time.xml' \
                'staging/content-type/content-type-v1.xml'
do
  protocolBasename=$(BasenameWithoutExtension "$protocol")
  protocolXml="$WaylandProtocolsDir/$protocol"
  if [ ! -e "$protocolXml" ]; then
    protocolXml="$ProjectRoot/protocol/$(Basename "$protocol")"
  fi

  wayland-scanner client-header "$protocolXml" "$outputDir/${protocolBasename}-client-protocol.h"
  wayland-scanner private-code "$protocolXml" "$outputDir/${protocolBasename}-protocol.c"

  WaylandProtocolsSrc="$WaylandProtocolsSrc $outputDir/${protocolBasename}-protocol.c"
done

WaylandProtocolsSrc="${WaylandProtocolsSrc## }"

Log "Wayland protocols generated in $(StopTimer) seconds"
