#!/bin/sh
export LC_ALL=C
export TZ=UTC

IsBuildDebug=1
IsBuildEnabled=1
IsTestsEnabled=1
IsToolEnabled_hh_record_read=0
IsToolEnabled_hh_asset_builder=0

# Possible values: stbtt, freetype
TruetypeBackend='stbtt'

################################################################
# TEXT FUNCTIONS
################################################################

# [0,1] StringContains(string, search)
StringContains() {
  string="$1"
  search="$2"
  [ "${string##*$search}" != "$string" ] && echo 1 || echo 0
}

# [0,1] StringStartsWith(string, search)
StringStartsWith() {
  string="$1"
  search="$2"
  [ "${string#$search}" != "$string" ] && echo 1 || echo 0
}

# [0,1] StringEndsWith(string, search)
StringEndsWith() {
  string="$1"
  search="$2"
  [ "${string%$search}" != "$string" ] && echo 1 || echo 0
}

# string Basename(path)
Basename() {
  path="$1"
  echo "${path##*/}"
}

# string BasenameWithoutExtension(path)
BasenameWithoutExtension() {
  path="$1"
  basename="$(Basename "$path")"
  echo ${basename%.*}
}

# string Dirname(path)
Dirname() {
  path="$1"
  if [ $(StringStartsWith "$path" '/') ]; then
    dirname="${path%/*}"
    if [ -z "$dirname" ]; then
      echo '/'
    else
      echo $dirname
    fi
  else
    echo '.'
  fi
}

################################################################
# TIME FUNCTIONS
################################################################

StartTimer() {
  startedAt=$(date +%s)
}

StopTimer() {
  echo $(( $(date +%s) - $startedAt ))
}

################################################################
# LOG FUNCTIONS
################################################################

Timestamp="$(date +%Y%m%dT%H%M%S)"

Log() {
  string=$1
  output="$OutputDir/logs/build-$Timestamp.log"
  if [ ! -e "$(Dirname "$output")" ]; then
    mkdir "$(Dirname "$output")"
  fi
  echo "$string" >> "$output"
}

Debug() {
  string=$1
  output="$OutputDir/logs/build-$Timestamp.log"
  if [ ! -e "$(Dirname "$output")" ]; then
    mkdir "$(Dirname "$output")"
  fi
  echo "[DEBUG] $string" >> "$output"
}

################################################################

ProjectRoot="$(Dirname $(realpath "$0"))"
if [ "$(pwd)" != "$ProjectRoot" ]; then
  echo "Must be call from project root!"
  echo "  $ProjectRoot"
  exit 1
fi

OutputDir="$ProjectRoot/build"
if [ ! -e "$OutputDir" ]; then
  mkdir "$OutputDir"

  # version control ignore
  echo '*' > "$OutputDir/.gitignore"

  echo 'syntax: glob' > "$OutputDir/.hgignore"
  echo '**/*' > "$OutputDir/.hgignore"
fi

IsOSLinux=$(StringEndsWith "$(uname)" 'Linux')

cc="${CC:-clang}"
IsCompilerGCC=$(StringStartsWith "$("$cc" --version | head -n 1 -c 32)" "gcc")
IsCompilerClang=$(StringStartsWith "$("$cc" --version | head -n 1 -c 32)" "clang")
if [ $IsCompilerGCC -eq 0 ] && [ $IsCompilerClang -eq 0 ]; then
  echo "unsupported compiler $cc. continue (y/n)?"
  read input
  if [ "$input" != 'y' ] && [ "$input" != 'Y' ]; then
    exit 1
  fi

  echo "Assuming $cc as GCC"
  IsCompilerGCC=1
fi

IsTruetypeBackendFreetype=$(test $TruetypeBackend = 'freetype' && echo 1 || echo 0)
IsTruetypeBackendSTBTT=$(test $TruetypeBackend = 'stbtt' && echo 1 || echo 0)

cflags="$CFLAGS"
# standard
cflags="$cflags -std=c99"
# performance
cflags="$cflags -O3"
if [ $(StringContains "$cflags" '-march=') -eq 0 ]; then
  cflags="$cflags -march=x86-64-v3"
fi
cflags="$cflags -funroll-loops"
cflags="$cflags -fomit-frame-pointer"
# warnings
cflags="$cflags -Wall -Werror"
cflags="$cflags -Wconversion"
cflags="$cflags -Wno-unused-parameter"
cflags="$cflags -Wno-unused-result"
cflags="$cflags -Wno-missing-braces"

cflags="$cflags -DCOMPILER_GCC=$IsCompilerGCC"
cflags="$cflags -DCOMPILER_CLANG=$IsCompilerClang"

if [ $IsBuildDebug -eq 1 ]; then
  #cflags="$cflags -g -O0"
  cflags="$cflags -DHANDMADEHERO_DEBUG=1"
  cflags="$cflags -DHANDMADEHERO_INTERNAL=1"
  cflags="$cflags -Wno-unused-but-set-variable"
  cflags="$cflags -Wno-unused-function"
  cflags="$cflags -Wno-unused-variable"
fi

ldflags="${LDFLAGS}"
ldflags="$ldflags -Wl,--as-needed"
ldflags="${ldflags# }"

Log "Started at $(date '+%Y-%m-%d %H:%M:%S')"
Log "================================================================"
Log "root:      $ProjectRoot"
Log "build:     $OutputDir"
Log "os:        $(uname)"
Log "compiler:  $cc"

Log "cflags: $cflags"
if [ ! -z "$CFLAGS" ]; then
  Log "from your env: $CFLAGS"
fi

Log "ldflags: $ldflags"
if [ ! -z "$LDFLAGS" ]; then
  Log "from your env: $LDFLAGS"
fi
Log "================================================================"

LIB_M='-lm'

if [ $IsBuildEnabled -eq 1 ]; then
  if [ $IsOSLinux -eq 0 ]; then
    echo "Do not know how to compile on this OS"
    echo "  OS: $(uname)"
    exit 1
  elif [ $IsOSLinux -eq 1 ]; then
    ################################################################
    # LINUX BUILD
    #      .--.
    #     |o_o |
    #     |:_/ |
    #    //   \ \
    #   (|     | )
    #  /'\_   _/`\
    #  \___)=(___/
    ################################################################
    LIB_PTHREAD='-lpthread'

    INC_LIBURING=$(pkg-config --cflags liburing)
    LIB_LIBURING=$(pkg-config --libs liburing)

    INC_LIBEVDEV=$(pkg-config --cflags libevdev)
    LIB_LIBEVDEV=$(pkg-config --libs libevdev)

    INC_WAYLAND_CLIENT=$(pkg-config --cflags wayland-client)
    LIB_WAYLAND_CLIENT=$(pkg-config --libs wayland-client)

    INC_XKBCOMMON=$(pkg-config --cflags xkbcommon)
    LIB_XKBCOMMON=$(pkg-config --libs xkbcommon)

    INC_LIBPIPEWIRE=$(pkg-config --cflags libpipewire-0.3)
    LIB_LIBPIPEWIRE=$(pkg-config --libs libpipewire-0.3)

    ### libhandmadehero
    src="$ProjectRoot/src/handmadehero.c"
    output="$OutputDir/libhandmadehero.so"
    inc="-I$ProjectRoot/include"
    lib="$LIB_M"
    StartTimer
    "$cc" $cflags $ldflags $inc -fPIC -shared -o "$output" $src $lib
    [ $? -eq 0 ] && echo "libhandmadehero compiled in $(StopTimer) seconds."

    ### handmadehero
    # WaylandProtocolsInc= WaylandProtocolsSrc=
    . $ProjectRoot/protocol/build.sh

    src=""
    src="$src $WaylandProtocolsSrc"
    src="$src $ProjectRoot/src/handmadehero_linux.c"
    src="${src# }"

    output="$OutputDir/handmadehero"
    inc="-I$ProjectRoot/include $INC_LIBURING $INC_LIBEVDEV $INC_WAYLAND_CLIENT $INC_XKBCOMMON $INC_LIBPIPEWIRE $WaylandProtocolsInc"
    lib="$LIB_M $LIB_PTHREAD $LIB_LIBURING $LIB_LIBEVDEV $LIB_WAYLAND_CLIENT $LIB_XKBCOMMON $LIB_LIBPIPEWIRE"
    StartTimer
    "$cc" $cflags $ldflags $inc -o "$output" $src $lib
    [ $? -eq 0 ] && echo "handmadehero compiled in $(StopTimer) seconds."
  fi
fi

if [ $IsTestsEnabled -eq 1 ]; then
  . "$ProjectRoot/test/build.sh"
fi

if [ $IsToolEnabled_hh_record_read -eq 1 ]; then
  . "$ProjectRoot/tool/hh_record_read/build.sh"
fi

if [ $IsToolEnabled_hh_asset_builder -eq 1 ]; then
  . "$ProjectRoot/tool/hh_asset_builder/build.sh"
fi

Log "================================================================"
Log "Finished at $(date '+%Y-%m-%d %H:%M:%S')"
