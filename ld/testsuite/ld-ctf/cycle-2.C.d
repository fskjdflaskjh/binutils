#as:
#source: A.c
#source: B.c
#source: C.c
#objdump: --ctf=.ctf
#ld: -shared --ctf-variables
#name: Cycle 2.C

.*: +file format .*

Contents of CTF section .ctf:

  Header:
    Magic number: dff2
    Version: 4 \(CTF_VERSION_3\)
#...
    Type section:	.* \(0x6c bytes\)
    String section:	.*

  Labels:

  Data objects:

  Function objects:

  Variables:
#...
    c ->  [0-9a-f]*: struct C \(.*
#...
  Types:
#...
     [0-9a-f]*: struct C \(.*
        \[0x0\] \(ID 0x[0-9a-f]*\) \(kind 6\) struct C \(.*
            \[0x0\] \(ID 0x[0-9a-f]*\) \(kind 3\) struct A \* a \(.*
#...
  Strings:
    0: 
#...
    [0-9a-f]*: C
#...
