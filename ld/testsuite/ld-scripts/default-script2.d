# source: default-script.s
# ld: -T default-script.t -defsym _START=0x800
# nm: -n
# skip: *-*-mingw64 x86_64-*-cygwin
# Skipped on Mingw64 and Cygwin because the image base defaults to 0x100000000

#...
0*800 . _START
#...
0*900 T text
#pass
