# Copyright (C) 2014-2019 Free Software Foundation, Inc.
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.
#
# Unusual variables checked by this code:
#	NOP - two byte opcode for no-op (defaults to 0)
#	INITIAL_READONLY_SECTIONS - at start of text segment
#	OTHER_READONLY_SECTIONS - other than .text .init .rodata ...
#		(e.g., .PARISC.milli)
#	OTHER_TEXT_SECTIONS - these get put in .text when relocating
#	OTHER_READWRITE_SECTIONS - other than .data .bss .ctors .sdata ...
#		(e.g., .PARISC.global)
#	OTHER_SECTIONS - at the end
#	EXECUTABLE_SYMBOLS - symbols that must be defined for an
#		executable (e.g., _DYNAMIC_LINK)
#	TEXT_START_SYMBOLS - symbols that appear at the start of the
#		.text section.
#	DATA_START_SYMBOLS - symbols that appear at the start of the
#		.data section.
#	OTHER_GOT_SYMBOLS - symbols defined just before .got.
#	OTHER_GOT_SECTIONS - sections just after .got and .sdata.
#	OTHER_BSS_SYMBOLS - symbols that appear at the start of the
#		.bss section besides __bss_start.
#	INPUT_FILES - INPUT command of files to always include
#	INIT_START, INIT_END -  statements just before and just after
#	combination of .init sections.
#	FINI_START, FINI_END - statements just before and just after
#	combination of .fini sections.
#
# When adding sections, do note that the names of some sections are used
# when specifying the start address of the next.
#

#  Many sections come in three flavours.  There is the 'real' section,
#  like ".data".  Then there are the per-procedure or per-variable
#  sections, generated by -ffunction-sections and -fdata-sections in GCC,
#  and useful for --gc-sections, which for a variable "foo" might be
#  ".data.foo".  Then there are the linkonce sections, for which the linker
#  eliminates duplicates, which are named like ".gnu.linkonce.d.foo".
#  The exact correspondences are:
#
#  Section	Linkonce section
#  .text	.gnu.linkonce.t.foo
#  .rodata	.gnu.linkonce.r.foo
#  .data	.gnu.linkonce.d.foo
#  .bss		.gnu.linkonce.b.foo
#  .sdata	.gnu.linkonce.s.foo
#  .sbss	.gnu.linkonce.sb.foo
#  .sdata2	.gnu.linkonce.s2.foo
#  .sbss2	.gnu.linkonce.sb2.foo
#
#  Each of these can also have corresponding .rel.* and .rela.* sections.

test -z "$ENTRY" && ENTRY=_start
test -z "${BIG_OUTPUT_FORMAT}" && BIG_OUTPUT_FORMAT=${OUTPUT_FORMAT}
test -z "${LITTLE_OUTPUT_FORMAT}" && LITTLE_OUTPUT_FORMAT=${OUTPUT_FORMAT}
if [ -z "$MACHINE" ]; then OUTPUT_ARCH=${ARCH}; else OUTPUT_ARCH=${ARCH}:${MACHINE}; fi
test -z "${ELFSIZE}" && ELFSIZE=32
test -z "${ALIGNMENT}" && ALIGNMENT="${ELFSIZE} / 8"
CTOR=".ctors ${CONSTRUCTING-0} :
  {
    ${CONSTRUCTING+${CTOR_START}}
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */

    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))

    /* We don't want to include the .ctor section from
       the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */

    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o $OTHER_EXCLUDE_FILES) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    ${CONSTRUCTING+${CTOR_END}}
  } > ROM"

DTOR=" .dtors       ${CONSTRUCTING-0} :
  {
    ${CONSTRUCTING+${DTOR_START}}
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o $OTHER_EXCLUDE_FILES) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    ${CONSTRUCTING+${DTOR_END}}
  } > ROM"

cat <<EOF
/* Copyright (C) 2014-2019 Free Software Foundation, Inc.

   Copying and distribution of this script, with or without modification,
   are permitted in any medium without royalty provided the copyright
   notice and this notice are preserved.  */

OUTPUT_FORMAT("${OUTPUT_FORMAT}", "${BIG_OUTPUT_FORMAT}",
	      "${LITTLE_OUTPUT_FORMAT}")
OUTPUT_ARCH(${OUTPUT_ARCH})
EOF

test -n "${RELOCATING}" && cat <<EOF
ENTRY(${ENTRY})

${LIB_SEARCH_DIRS}
${EXECUTABLE_SYMBOLS}
${INPUT_FILES}

/* There are two memory regions we care about, one from 0 through 0x7F00
   that is RAM and one from 0x8000 up which is ROM.  */
MEMORY
{
  RAM (w) : ORIGIN = 0, LENGTH = 0x7F00
  ROM (!w) : ORIGIN = 0x8000, LENGTH = 0xFF8000
}
EOF

cat <<EOF
SECTIONS
{
  .data  ${RELOCATING-0} :
  {
    ${RELOCATING+__rdata = .;}
    ${RELOCATING+__data = .;}
    ${RELOCATING+${DATA_START_SYMBOLS}}
    *(.data)
    ${RELOCATING+*(.data.*)}
    ${RELOCATING+*(.gnu.linkonce.d.*)}
    ${CONSTRUCTING+SORT(CONSTRUCTORS)}
  }${RELOCATING+ > RAM}
  ${RELOCATING+${OTHER_READWRITE_SECTIONS}}
  ${RELOCATING+${OTHER_GOT_SYMBOLS}}
  ${RELOCATING+${OTHER_GOT_SECTIONS}}
  ${RELOCATING+_edata = .;}
  ${RELOCATING+PROVIDE (edata = .);}
  ${RELOCATING+__bss_start = .;}
  ${RELOCATING+${OTHER_BSS_SYMBOLS}}
  .bss     ${RELOCATING-0} :
  {
   ${RELOCATING+*(.dynbss)}
   *(.bss)
   ${RELOCATING+*(.bss.*)}
   ${RELOCATING+*(.gnu.linkonce.b.*)}
   ${RELOCATING+*(COMMON)
   /* Align here to ensure that the .bss section occupies space up to
      _end.  Align after .bss to ensure correct alignment even if the
      .bss section disappears because there are no input sections.  */
   . = ALIGN(${ALIGNMENT});}
  }${RELOCATING+ > RAM}
  ${RELOCATING+${OTHER_BSS_END_SYMBOLS}}
  ${RELOCATING+. = ALIGN(${ALIGNMENT});}
  ${RELOCATING+${OTHER_END_SYMBOLS}}
  ${RELOCATING+_end = .;}
  ${RELOCATING+__stack = .;}
  ${RELOCATING+PROVIDE (end = .);}

  /* Read-only sections in ROM.  */
  .int_vec     ${RELOCATING-0} : { *(.int_vec)	}${RELOCATING+ > ROM}

  .rodata ${RELOCATING-0} : { *(.rodata) ${RELOCATING+*(.rodata.*)} ${RELOCATING+*(.gnu.linkonce.r.*)} }${RELOCATING+ > ROM}
  ${RELOCATING+${CTOR}}
  ${RELOCATING+${DTOR}}
  .jcr : { KEEP (*(.jcr)) }${RELOCATING+ > ROM}
  .eh_frame : { KEEP (*(.eh_frame)) }${RELOCATING+ > ROM}
  .gcc_except_table : { *(.gcc_except_table)${RELOCATING+ *(.gcc_except_table.*)} }${RELOCATING+ > ROM}
  .plt : { *(.plt) }${RELOCATING+ > ROM}

  .text    ${RELOCATING-0} :
  {
    ${RELOCATING+${TEXT_START_SYMBOLS}}
    *(.text)
    ${RELOCATING+*(.text.*)}
    *(.stub)
    /* .gnu.warning sections are handled specially by elf.em.  */
    *(.gnu.warning)
    ${RELOCATING+*(.gnu.linkonce.t.*)}
    ${RELOCATING+${OTHER_TEXT_SECTIONS}}
  }${RELOCATING+ > ROM =${NOP-0}}
  .init        ${RELOCATING-0} :
  {
    ${RELOCATING+${INIT_START}}
    KEEP (*(SORT_NONE(.init)))
    ${RELOCATING+${INIT_END}}
  }${RELOCATING+ > ROM =${NOP-0}}
  .fini    ${RELOCATING-0} :
  {
    ${RELOCATING+${FINI_START}}
    KEEP (*(SORT_NONE(.fini)))
    ${RELOCATING+${FINI_END}}
  }${RELOCATING+ > ROM =${NOP-0}}
  ${RELOCATING+PROVIDE (__etext = .);}
  ${RELOCATING+PROVIDE (_etext = .);}
  ${RELOCATING+PROVIDE (etext = .);}
  ${RELOCATING+${OTHER_READONLY_SECTIONS}}


  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }

  .comment 0 : { *(.comment) }

EOF

. $srcdir/scripttempl/DWARF.sc

cat <<EOF
  ${RELOCATING+${OTHER_RELOCATING_SECTIONS}}

  /* These must appear regardless of ${RELOCATING}.  */
  ${OTHER_SECTIONS}
}
EOF
