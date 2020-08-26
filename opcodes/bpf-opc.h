/* DO NOT EDIT!  -*- buffer-read-only: t -*- vi:set ro:  */
/* Instruction opcode header for bpf.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright (C) 1996-2020 Free Software Foundation, Inc.

This file is part of the GNU Binutils and/or GDB, the GNU debugger.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   It is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.

*/

#ifndef BPF_OPC_H
#define BPF_OPC_H

#ifdef __cplusplus
extern "C" {
#endif

/* -- opc.h */

#undef CGEN_DIS_HASH_SIZE
#define CGEN_DIS_HASH_SIZE 1

#undef CGEN_DIS_HASH
#define CGEN_DIS_HASH(buffer, value) 0

/* Allows reason codes to be output when assembler errors occur.  */
#define CGEN_VERBOSE_ASSEMBLER_ERRORS

#define CGEN_VALIDATE_INSN_SUPPORTED
extern int bpf_cgen_insn_supported (CGEN_CPU_DESC, const CGEN_INSN *);


/* -- opc.c */
/* Enum declaration for bpf instruction types.  */
typedef enum cgen_insn_type {
  BPF_INSN_INVALID, BPF_INSN_ADDILE, BPF_INSN_ADDRLE, BPF_INSN_ADD32ILE
 , BPF_INSN_ADD32RLE, BPF_INSN_SUBILE, BPF_INSN_SUBRLE, BPF_INSN_SUB32ILE
 , BPF_INSN_SUB32RLE, BPF_INSN_MULILE, BPF_INSN_MULRLE, BPF_INSN_MUL32ILE
 , BPF_INSN_MUL32RLE, BPF_INSN_DIVILE, BPF_INSN_DIVRLE, BPF_INSN_DIV32ILE
 , BPF_INSN_DIV32RLE, BPF_INSN_ORILE, BPF_INSN_ORRLE, BPF_INSN_OR32ILE
 , BPF_INSN_OR32RLE, BPF_INSN_ANDILE, BPF_INSN_ANDRLE, BPF_INSN_AND32ILE
 , BPF_INSN_AND32RLE, BPF_INSN_LSHILE, BPF_INSN_LSHRLE, BPF_INSN_LSH32ILE
 , BPF_INSN_LSH32RLE, BPF_INSN_RSHILE, BPF_INSN_RSHRLE, BPF_INSN_RSH32ILE
 , BPF_INSN_RSH32RLE, BPF_INSN_MODILE, BPF_INSN_MODRLE, BPF_INSN_MOD32ILE
 , BPF_INSN_MOD32RLE, BPF_INSN_XORILE, BPF_INSN_XORRLE, BPF_INSN_XOR32ILE
 , BPF_INSN_XOR32RLE, BPF_INSN_ARSHILE, BPF_INSN_ARSHRLE, BPF_INSN_ARSH32ILE
 , BPF_INSN_ARSH32RLE, BPF_INSN_NEGLE, BPF_INSN_NEG32LE, BPF_INSN_MOVILE
 , BPF_INSN_MOVRLE, BPF_INSN_MOV32ILE, BPF_INSN_MOV32RLE, BPF_INSN_ADDIBE
 , BPF_INSN_ADDRBE, BPF_INSN_ADD32IBE, BPF_INSN_ADD32RBE, BPF_INSN_SUBIBE
 , BPF_INSN_SUBRBE, BPF_INSN_SUB32IBE, BPF_INSN_SUB32RBE, BPF_INSN_MULIBE
 , BPF_INSN_MULRBE, BPF_INSN_MUL32IBE, BPF_INSN_MUL32RBE, BPF_INSN_DIVIBE
 , BPF_INSN_DIVRBE, BPF_INSN_DIV32IBE, BPF_INSN_DIV32RBE, BPF_INSN_ORIBE
 , BPF_INSN_ORRBE, BPF_INSN_OR32IBE, BPF_INSN_OR32RBE, BPF_INSN_ANDIBE
 , BPF_INSN_ANDRBE, BPF_INSN_AND32IBE, BPF_INSN_AND32RBE, BPF_INSN_LSHIBE
 , BPF_INSN_LSHRBE, BPF_INSN_LSH32IBE, BPF_INSN_LSH32RBE, BPF_INSN_RSHIBE
 , BPF_INSN_RSHRBE, BPF_INSN_RSH32IBE, BPF_INSN_RSH32RBE, BPF_INSN_MODIBE
 , BPF_INSN_MODRBE, BPF_INSN_MOD32IBE, BPF_INSN_MOD32RBE, BPF_INSN_XORIBE
 , BPF_INSN_XORRBE, BPF_INSN_XOR32IBE, BPF_INSN_XOR32RBE, BPF_INSN_ARSHIBE
 , BPF_INSN_ARSHRBE, BPF_INSN_ARSH32IBE, BPF_INSN_ARSH32RBE, BPF_INSN_NEGBE
 , BPF_INSN_NEG32BE, BPF_INSN_MOVIBE, BPF_INSN_MOVRBE, BPF_INSN_MOV32IBE
 , BPF_INSN_MOV32RBE, BPF_INSN_ENDLELE, BPF_INSN_ENDBELE, BPF_INSN_ENDLEBE
 , BPF_INSN_ENDBEBE, BPF_INSN_LDDWLE, BPF_INSN_LDDWBE, BPF_INSN_LDABSW
 , BPF_INSN_LDABSH, BPF_INSN_LDABSB, BPF_INSN_LDABSDW, BPF_INSN_LDINDWLE
 , BPF_INSN_LDINDHLE, BPF_INSN_LDINDBLE, BPF_INSN_LDINDDWLE, BPF_INSN_LDINDWBE
 , BPF_INSN_LDINDHBE, BPF_INSN_LDINDBBE, BPF_INSN_LDINDDWBE, BPF_INSN_LDXWLE
 , BPF_INSN_LDXHLE, BPF_INSN_LDXBLE, BPF_INSN_LDXDWLE, BPF_INSN_STXWLE
 , BPF_INSN_STXHLE, BPF_INSN_STXBLE, BPF_INSN_STXDWLE, BPF_INSN_LDXWBE
 , BPF_INSN_LDXHBE, BPF_INSN_LDXBBE, BPF_INSN_LDXDWBE, BPF_INSN_STXWBE
 , BPF_INSN_STXHBE, BPF_INSN_STXBBE, BPF_INSN_STXDWBE, BPF_INSN_STBLE
 , BPF_INSN_STHLE, BPF_INSN_STWLE, BPF_INSN_STDWLE, BPF_INSN_STBBE
 , BPF_INSN_STHBE, BPF_INSN_STWBE, BPF_INSN_STDWBE, BPF_INSN_JEQILE
 , BPF_INSN_JEQRLE, BPF_INSN_JEQ32ILE, BPF_INSN_JEQ32RLE, BPF_INSN_JGTILE
 , BPF_INSN_JGTRLE, BPF_INSN_JGT32ILE, BPF_INSN_JGT32RLE, BPF_INSN_JGEILE
 , BPF_INSN_JGERLE, BPF_INSN_JGE32ILE, BPF_INSN_JGE32RLE, BPF_INSN_JLTILE
 , BPF_INSN_JLTRLE, BPF_INSN_JLT32ILE, BPF_INSN_JLT32RLE, BPF_INSN_JLEILE
 , BPF_INSN_JLERLE, BPF_INSN_JLE32ILE, BPF_INSN_JLE32RLE, BPF_INSN_JSETILE
 , BPF_INSN_JSETRLE, BPF_INSN_JSET32ILE, BPF_INSN_JSET32RLE, BPF_INSN_JNEILE
 , BPF_INSN_JNERLE, BPF_INSN_JNE32ILE, BPF_INSN_JNE32RLE, BPF_INSN_JSGTILE
 , BPF_INSN_JSGTRLE, BPF_INSN_JSGT32ILE, BPF_INSN_JSGT32RLE, BPF_INSN_JSGEILE
 , BPF_INSN_JSGERLE, BPF_INSN_JSGE32ILE, BPF_INSN_JSGE32RLE, BPF_INSN_JSLTILE
 , BPF_INSN_JSLTRLE, BPF_INSN_JSLT32ILE, BPF_INSN_JSLT32RLE, BPF_INSN_JSLEILE
 , BPF_INSN_JSLERLE, BPF_INSN_JSLE32ILE, BPF_INSN_JSLE32RLE, BPF_INSN_JEQIBE
 , BPF_INSN_JEQRBE, BPF_INSN_JEQ32IBE, BPF_INSN_JEQ32RBE, BPF_INSN_JGTIBE
 , BPF_INSN_JGTRBE, BPF_INSN_JGT32IBE, BPF_INSN_JGT32RBE, BPF_INSN_JGEIBE
 , BPF_INSN_JGERBE, BPF_INSN_JGE32IBE, BPF_INSN_JGE32RBE, BPF_INSN_JLTIBE
 , BPF_INSN_JLTRBE, BPF_INSN_JLT32IBE, BPF_INSN_JLT32RBE, BPF_INSN_JLEIBE
 , BPF_INSN_JLERBE, BPF_INSN_JLE32IBE, BPF_INSN_JLE32RBE, BPF_INSN_JSETIBE
 , BPF_INSN_JSETRBE, BPF_INSN_JSET32IBE, BPF_INSN_JSET32RBE, BPF_INSN_JNEIBE
 , BPF_INSN_JNERBE, BPF_INSN_JNE32IBE, BPF_INSN_JNE32RBE, BPF_INSN_JSGTIBE
 , BPF_INSN_JSGTRBE, BPF_INSN_JSGT32IBE, BPF_INSN_JSGT32RBE, BPF_INSN_JSGEIBE
 , BPF_INSN_JSGERBE, BPF_INSN_JSGE32IBE, BPF_INSN_JSGE32RBE, BPF_INSN_JSLTIBE
 , BPF_INSN_JSLTRBE, BPF_INSN_JSLT32IBE, BPF_INSN_JSLT32RBE, BPF_INSN_JSLEIBE
 , BPF_INSN_JSLERBE, BPF_INSN_JSLE32IBE, BPF_INSN_JSLE32RBE, BPF_INSN_CALLLE
 , BPF_INSN_CALLBE, BPF_INSN_CALLRLE, BPF_INSN_CALLRBE, BPF_INSN_JA
 , BPF_INSN_EXIT, BPF_INSN_XADDDWLE, BPF_INSN_XADDWLE, BPF_INSN_XADDDWBE
 , BPF_INSN_XADDWBE, BPF_INSN_BRKPT
} CGEN_INSN_TYPE;

/* Index of `invalid' insn place holder.  */
#define CGEN_INSN_INVALID BPF_INSN_INVALID

/* Total number of insns in table.  */
#define MAX_INSNS ((int) BPF_INSN_BRKPT + 1)

/* This struct records data prior to insertion or after extraction.  */
struct cgen_fields
{
  int length;
  long f_nil;
  long f_anyof;
  long f_op_code;
  long f_op_src;
  long f_op_class;
  long f_op_mode;
  long f_op_size;
  long f_dstle;
  long f_srcle;
  long f_dstbe;
  long f_srcbe;
  long f_regs;
  long f_offset16;
  long f_imm32;
  long f_imm64_a;
  long f_imm64_b;
  long f_imm64_c;
  int64_t f_imm64;
};

#define CGEN_INIT_PARSE(od) \
{\
}
#define CGEN_INIT_INSERT(od) \
{\
}
#define CGEN_INIT_EXTRACT(od) \
{\
}
#define CGEN_INIT_PRINT(od) \
{\
}


   #ifdef __cplusplus
   }
   #endif

#endif /* BPF_OPC_H */
