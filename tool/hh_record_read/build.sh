inc="-I$ProjectRoot/include"
src="$ProjectRoot/tool/hh_record_read/main.c"
output="$OutputDir/hh_record_read"
lib=""

StartTimer
"$cc" $cflags $ldflags $inc -o "$output" $src $lib
[ $? -eq 0 ] && Log "hh_record_read compiled in $(StopTimer) seconds."
