/* Freescale S12Z-specific support for 32-bit ELF
   Copyright (C) 1999-2019 Free Software Foundation, Inc.
   (Heavily copied from the D10V port by Martin Hunt (hunt@cygnus.com))

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"

#include "elf/s12z.h"

/* Relocation functions.  */
static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  (bfd *, bfd_reloc_code_real_type);
static bfd_boolean s12z_info_to_howto_rel
  (bfd *, arelent *, Elf_Internal_Rela *);

static bfd_reloc_status_type
opru18_reloc (bfd *abfd, arelent *reloc_entry, struct bfd_symbol *symbol,
		    void *data, asection *input_section ATTRIBUTE_UNUSED,
		    bfd *output ATTRIBUTE_UNUSED, char **msg ATTRIBUTE_UNUSED)
{
  /* This reloc is used for 18 bit General Operand Addressing Postbyte in the
     INST opru18 form.  This is an 18 bit reloc, but the most significant bit
     is shifted one place to the left of where it would normally be.  See
     Appendix A.4 of the S12Z reference manual.  */

  bfd_size_type octets = reloc_entry->address * bfd_octets_per_byte (abfd);
  bfd_vma result = bfd_get_24 (abfd, (unsigned char *) data + octets);
  bfd_vma val = bfd_asymbol_value (symbol);

  /* Keep the wanted bits and discard the rest.  */
  result &= 0xFA0000;

  val += symbol->section->output_section->vma;
  val += symbol->section->output_offset;

  /* The lowest 17 bits are copied verbatim.  */
  result |= val & 0x1FFFF;

  /* The 18th bit is put into the 19th position.  */
  result |= (val & 0x020000) << 1;

  bfd_put_24 (abfd, result, (unsigned char *) data + octets);

  return bfd_reloc_ok;
}


static bfd_reloc_status_type
shift_addend_reloc (bfd *abfd, arelent *reloc_entry, struct bfd_symbol *symbol ATTRIBUTE_UNUSED,
		    void *data ATTRIBUTE_UNUSED, asection *input_section ATTRIBUTE_UNUSED,
		    bfd *output ATTRIBUTE_UNUSED, char **msg ATTRIBUTE_UNUSED)
{
  /* This is a really peculiar reloc, which is done for compatibility
     with the Freescale toolchain.

     That toolchain appears to (ab)use the lowest 15 bits of the addend for
     the purpose of holding flags.  The purpose of these flags are unknown.
     So in this function, when writing the bfd we left shift the addend by
     15, and when reading we right shift it by 15 (discarding the lower bits).

     This allows the linker to work with object files generated by Freescale,
     as well as by Gas.  */

  if (abfd->is_linker_input)
    reloc_entry->addend >>= 15;
  else
    reloc_entry->addend <<= 15;

  return bfd_reloc_continue;
}

#define USE_REL	0

static reloc_howto_type elf_s12z_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_S12Z_NONE,	/* type */
	 0,			/* rightshift */
	 3,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_S12Z_NONE",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 24 bit absolute relocation emitted by the OPR mode operands  */
  HOWTO (R_S12Z_OPR,        /* type */
	 0,			/* rightshift */
	 5,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 shift_addend_reloc,
	 "R_S12Z_OPR",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00ffffff,            /* src_mask */
	 0x00ffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* The purpose of this reloc is not known */
  HOWTO (R_S12Z_UKNWN_2,	/* type */
	 0,			/* rightshift */
	 3,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont,/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_S12Z_UKNWN_2",	/* name */
	 FALSE,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 15 bit PC-rel relocation */
  HOWTO (R_S12Z_PCREL_7_15,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 15,			/* bitsize */
	 TRUE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 shift_addend_reloc,
	 "R_S12Z_PCREL_7_15",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00,                  /* src_mask */
	 0x007fff,		/* dst_mask */
	 TRUE),		/* pcrel_offset */

  /* A 24 bit absolute relocation emitted by EXT24 mode operands */
  HOWTO (R_S12Z_EXT24,        /* type */
	 0,			/* rightshift */
	 5,			/* size (0 = byte, 1 = short, 2 = long) */
	 24,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_S12Z_EXT24",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,            /* src_mask */
	 0x00ffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* An 18 bit absolute relocation */
  HOWTO (R_S12Z_EXT18,        /* type */
	 0,			/* rightshift */
	 5,			/* size (0 = byte, 1 = short, 2 = long) */
	 18,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 opru18_reloc,	        /* special_function */
	 "R_S12Z_EXT18",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,            /* src_mask */
	 0x0005ffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation.   This kind of relocation is
     schizophrenic - Although they appear in sections named .rela.debug.*
     in some sections they behave as RELA relocs, but in others they have
     an added of zero and behave as REL.

     It is not recommended that new code  emits this reloc.   It is here
     only to support existing elf files generated by third party
     applications.  */

  HOWTO (R_S12Z_CW32,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_S12Z_CW32",	/* name */
	 FALSE,			/* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */

  /* A 32 bit absolute relocation  */
  HOWTO (R_S12Z_EXT32,        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 FALSE,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield,	/* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_S12Z_EXT32",	/* name */
	 FALSE,			/* partial_inplace */
	 0x00000000,            /* src_mask */
	 0xffffffff,		/* dst_mask */
	 FALSE),		/* pcrel_offset */
};

/* Map BFD reloc types to S12Z ELF reloc types.  */

struct s12z_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct s12z_reloc_map s12z_reloc_map[] =
{
  /* bfd reloc val */  /* elf reloc val */
  {BFD_RELOC_NONE,     R_S12Z_NONE},
  {BFD_RELOC_32,       R_S12Z_EXT32},
  {BFD_RELOC_24,       R_S12Z_EXT24},
  {BFD_RELOC_16_PCREL, R_S12Z_PCREL_7_15},
  {BFD_RELOC_S12Z_OPR, R_S12Z_OPR}
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
				 bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = 0;
       i < sizeof (s12z_reloc_map) / sizeof (struct s12z_reloc_map);
       i++)
    {
      if (s12z_reloc_map[i].bfd_reloc_val == code)
	{
	  return &elf_s12z_howto_table[s12z_reloc_map[i].elf_reloc_val];
	}
    }

  printf ("%s:%d Not found type %d\n", __FILE__, __LINE__, code);

  return NULL;
}

static reloc_howto_type *
bfd_elf32_bfd_reloc_name_lookup (bfd *abfd ATTRIBUTE_UNUSED,
				 const char *r_name)
{
  unsigned int i;

  for (i = 0;
       i < (sizeof (elf_s12z_howto_table)
	    / sizeof (elf_s12z_howto_table[0]));
       i++)
    if (elf_s12z_howto_table[i].name != NULL
	&& strcasecmp (elf_s12z_howto_table[i].name, r_name) == 0)
      return &elf_s12z_howto_table[i];

  return NULL;
}

/* Set the howto pointer for an S12Z ELF reloc.  */

static bfd_boolean
s12z_info_to_howto_rel (bfd *abfd,
			  arelent *cache_ptr, Elf_Internal_Rela *dst)
{
  unsigned int  r_type = ELF32_R_TYPE (dst->r_info);

  if (r_type >= (unsigned int) R_S12Z_max)
    {
      /* xgettext:c-format */
      _bfd_error_handler (_("%pB: unsupported relocation type %#x"),
			  abfd, r_type);
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  cache_ptr->howto = &elf_s12z_howto_table[r_type];
  return TRUE;
}

static bfd_boolean
s12z_elf_set_mach_from_flags (bfd *abfd)
{
  bfd_default_set_arch_mach (abfd, bfd_arch_s12z, 0);

  return TRUE;
}

#define ELF_ARCH		bfd_arch_s12z
#define ELF_MACHINE_CODE	EM_S12Z
#define ELF_MAXPAGESIZE		0x1000

#define TARGET_BIG_SYM		s12z_elf32_vec
#define TARGET_BIG_NAME		"elf32-s12z"

#define elf_info_to_howto			NULL
#define elf_info_to_howto_rel			s12z_info_to_howto_rel
#define elf_backend_object_p			s12z_elf_set_mach_from_flags

#include "elf32-target.h"
