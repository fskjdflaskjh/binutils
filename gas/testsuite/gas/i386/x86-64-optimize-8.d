#as: -O2 -march=+noavx
#objdump: -drw
#name: x86-64 optimized encoding 8 with -O2

.*: +file format .*


Disassembly of section .text:

0+ <_start>:
 +[a-f0-9]+:	62 f1 7d 28 6f d1    	vmovdqa32 %ymm1,%ymm2
#pass
