#name: Z80N arch test
#source: dummy1.s -z80n
#source: dummy2.s -z80n
#ld: -e 0
#objdump: -f

.*:[     ]+file format (coff|elf32)\-z80
architecture: z80n, flags 0x[0-9a-fA-F]+:
.*
.*

