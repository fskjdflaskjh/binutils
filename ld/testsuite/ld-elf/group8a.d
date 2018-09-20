#source: group8.s
#ld: -r --gc-sections --entry foo
#readelf: -g --wide
#xfail: cr16-*-* crx-*-* d30v-*-* dlx-*-* hppa64-*-*
#xfail: mep-*-* mn10200-*-* pj*-*-* pru-*-* xgate-*-*
# generic linker targets don't support --gc-sections, nor do a bunch of others
# cr16 and crx use non-standard scripts with memory regions, which don't play
# well with unique group sections under ld -r.

COMDAT group section \[[ 0-9]+\] `.group' \[foo\] contains . sections:
   \[Index\]    Name
   \[[ 0-9]+\]   .text.foo
#pass
