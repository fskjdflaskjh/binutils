#name: strings test
#objdump: -s -j .data

.*:[     ]+file format (coff)|(elf32)\-z80

Contents of section \.data:
 0000 2e646220 74657874 5c6e3833 37343830.*
 0010 44454642 20746578 745c6e64 38373833.*
 0020 4445464d 20746578 745c6e33 37383537.*
 0030 44422074 6578745c 6e333837 39383337.*
 0040 2e617363 69692074 6578743a bf0c0a00.*
 0050 2e617363 697a2074 6578740a 39393900.*
 0060 2e737472 696e6720 74657874 0a090000.*
#pass
