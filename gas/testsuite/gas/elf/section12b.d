#source: section12.s
#as: --no-pad-sections
#objdump: -s
#name: mbind section contents
# RX annoyingly reorders the sections so that they do not match the sequence
# expected below.  msp430 and hpux do not support SHF_GNU_MBIND
#xfail: rx-*-* msp430-*-* *-*-hpux*

#...
Contents of section .mbind.data:
 0000 01                                   .               
#...
Contents of section .mbind.data:
 0000 02                                   .               
#...
Contents of section .mbind.text:
 0000 03                                   .               
#...
Contents of section .mbind.text:
 0000 04                                   .               
#...
Contents of section .mbind.rodata:
 0000 07                                   .               
#...
Contents of section .mbind.data:
 0000 08                                   .               
#...
Contents of section .mbind.data:
 0000 090b                                 ..              
#...
Contents of section .mbind.text:
 0000 0a                                   .               
#pass
