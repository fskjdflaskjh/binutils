#source: start.s
#readelf: -d -s -D
#ld: -shared --hash-style=gnu
#target: *-*-linux* *-*-gnu* arm*-*-uclinuxfdpiceabi
#xfail: mips*-*-*

#...
 +0x[0-9a-z]+ +\(GNU_HASH\) +0x[0-9a-z]+
#...
 +[0-9]+ +[0-9]+: +[0-9a-f]+ +[0-9]+ +NOTYPE +GLOBAL +DEFAULT +[1-9] _start
#...
 +[0-9]+ +[0-9]+: +[0-9a-f]+ +[0-9]+ +NOTYPE +GLOBAL +DEFAULT +[1-9] main
#...
 +[0-9]+ +[0-9]+: +[0-9a-f]+ +[0-9]+ +NOTYPE +GLOBAL +DEFAULT +[1-9] start
#...
 +[0-9]+ +[0-9]+: +[0-9a-f]+ +[0-9]+ +NOTYPE +GLOBAL +DEFAULT +[1-9] __start
#...
