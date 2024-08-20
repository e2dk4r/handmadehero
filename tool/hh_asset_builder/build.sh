inc="-I$ProjectRoot/include"
src="$ProjectRoot/tool/hh_asset_builder/main.c"
output="$OutputDir/hh_asset_builder"
lib="$LIB_M"

if [ $IsTruetypeBackendFreetype -eq 0 ] && [ $IsTruetypeBackendSTBTT -eq 0 ]; then
  echo "Truetype backend is invalid $TruetypeBackend"
  exit 1
fi

if [ $TruetypeBackend = 'freetype' ]; then
  INC_FREETYPE2=$(pkg-config --cflags freetype2)
  LIB_FREETYPE2=$(pkg-config --libs freetype2)
  inc="$inc $INC_FREETYPE2"
  lib="$lib $LIB_FREETYPE2"
fi

StartTimer

cflagsSpecial="$cflagsSpecial -DTRUETYPE_BACKEND_FREETYPE=$IsTruetypeBackendFreetype"
cflagsSpecial="$cflagsSpecial -DTRUETYPE_BACKEND_STBTT=$IsTruetypeBackendSTBTT"
cflagsSpecial="${cflagsSpecial# }"

"$cc" $cflags $cflagsSpecial $ldflags $inc -o "$output" $src $lib
[ $? -eq 0 ] && Log "hh_asset_builder compiled in $(StopTimer) seconds."
