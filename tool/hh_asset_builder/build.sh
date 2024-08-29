# vi: set et ft=sh ts=2 sw=2 fenc=utf-8 :vi
if [ $IsTruetypeBackendFreetype -eq 0 ] && [ $IsTruetypeBackendSTBTT -eq 0 ]; then
  echo ASSERTION failed, truetype backend is not valid
  exit 1
fi

inc="-I$ProjectRoot/include"
src="$ProjectRoot/tool/hh_asset_builder/main.c"
output="$OutputDir/hh_asset_builder"
lib="$LIB_M"

if [ $IsTruetypeBackendFreetype -eq 1 ]; then
  INC_FREETYPE2=$(pkg-config --cflags freetype2)
  LIB_FREETYPE2=$(pkg-config --libs freetype2)
  inc="$inc $INC_FREETYPE2"
  lib="$lib $LIB_FREETYPE2"
fi

StartTimer

cflagsExtra=""
cflagsExtra="$cflagsExtra -DTRUETYPE_BACKEND_FREETYPE=$IsTruetypeBackendFreetype"
cflagsExtra="$cflagsExtra -DTRUETYPE_BACKEND_STBTT=$IsTruetypeBackendSTBTT"
cflagsExtra="${cflagsExtra# }"

"$cc" $cflags $cflagsExtra $ldflags $inc -o "$output" $src $lib
[ $? -eq 0 ] && echo "hh_asset_builder compiled in $(StopTimer) seconds."
