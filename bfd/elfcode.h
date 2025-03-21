/* ELF executable support for BFD.
   Copyright (C) 1991-2025 Free Software Foundation, Inc.

   Written by Fred Fish @ Cygnus Support, from information published
   in "UNIX System V Release 4, Programmers Guide: ANSI C and
   Programming Support Tools".  Sufficient support for gdb.

   Rewritten by Mark Eichin @ Cygnus Support, from information
   published in "System V Application Binary Interface", chapters 4
   and 5, as well as the various "Processor Supplement" documents
   derived from it. Added support for assembler and other object file
   utilities.  Further work done by Ken Raeburn (Cygnus Support), Michael
   Meissner (Open Software Foundation), and Peter Hoogenboom (University
   of Utah) to finish and extend this.

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


/* Problems and other issues to resolve.

   (1)	BFD expects there to be some fixed number of "sections" in
	the object file.  I.E. there is a "section_count" variable in the
	bfd structure which contains the number of sections.  However, ELF
	supports multiple "views" of a file.  In particular, with current
	implementations, executable files typically have two tables, a
	program header table and a section header table, both of which
	partition the executable.

	In ELF-speak, the "linking view" of the file uses the section header
	table to access "sections" within the file, and the "execution view"
	uses the program header table to access "segments" within the file.
	"Segments" typically may contain all the data from one or more
	"sections".

	Note that the section header table is optional in ELF executables,
	but it is this information that is most useful to gdb.  If the
	section header table is missing, then gdb should probably try
	to make do with the program header table.  (FIXME)

   (2)  The code in this file is compiled twice, once in 32-bit mode and
	once in 64-bit mode.  More of it should be made size-independent
	and moved into elf.c.

   (3)	ELF section symbols are handled rather sloppily now.  This should
	be cleaned up, and ELF section symbols reconciled with BFD section
	symbols.

   (4)  We need a published spec for 64-bit ELF.  We've got some stuff here
	that we're using for SPARC V9 64-bit chips, but don't assume that
	it's cast in stone.
 */

#include "sysdep.h"
#include "bfd.h"
#include "libiberty.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "libiberty.h"

/* Renaming structures, typedefs, macros and functions to be size-specific.  */
#define Elf_External_Ehdr	NAME(Elf,External_Ehdr)
#define Elf_External_Sym	NAME(Elf,External_Sym)
#define Elf_External_Shdr	NAME(Elf,External_Shdr)
#define Elf_External_Phdr	NAME(Elf,External_Phdr)
#define Elf_External_Rel	NAME(Elf,External_Rel)
#define Elf_External_Rela	NAME(Elf,External_Rela)
#define Elf_External_Dyn	NAME(Elf,External_Dyn)

#define elf_core_file_failing_command	NAME(bfd_elf,core_file_failing_command)
#define elf_core_file_failing_signal	NAME(bfd_elf,core_file_failing_signal)
#define elf_core_file_matches_executable_p \
  NAME(bfd_elf,core_file_matches_executable_p)
#define elf_core_file_pid		NAME(bfd_elf,core_file_pid)
#define elf_object_p			NAME(bfd_elf,object_p)
#define elf_core_file_p			NAME(bfd_elf,core_file_p)
#define elf_get_symtab_upper_bound	NAME(bfd_elf,get_symtab_upper_bound)
#define elf_get_dynamic_symtab_upper_bound \
  NAME(bfd_elf,get_dynamic_symtab_upper_bound)
#define elf_swap_reloc_in		NAME(bfd_elf,swap_reloc_in)
#define elf_swap_reloca_in		NAME(bfd_elf,swap_reloca_in)
#define elf_swap_reloc_out		NAME(bfd_elf,swap_reloc_out)
#define elf_swap_reloca_out		NAME(bfd_elf,swap_reloca_out)
#define elf_swap_symbol_in		NAME(bfd_elf,swap_symbol_in)
#define elf_swap_symbol_out		NAME(bfd_elf,swap_symbol_out)
#define elf_swap_phdr_in		NAME(bfd_elf,swap_phdr_in)
#define elf_swap_phdr_out		NAME(bfd_elf,swap_phdr_out)
#define elf_swap_dyn_in			NAME(bfd_elf,swap_dyn_in)
#define elf_swap_dyn_out		NAME(bfd_elf,swap_dyn_out)
#define elf_get_reloc_upper_bound	NAME(bfd_elf,get_reloc_upper_bound)
#define elf_canonicalize_reloc		NAME(bfd_elf,canonicalize_reloc)
#define elf_slurp_symbol_table		NAME(bfd_elf,slurp_symbol_table)
#define elf_canonicalize_symtab		NAME(bfd_elf,canonicalize_symtab)
#define elf_canonicalize_dynamic_symtab \
  NAME(bfd_elf,canonicalize_dynamic_symtab)
#define elf_get_synthetic_symtab \
  NAME(bfd_elf,get_synthetic_symtab)
#define elf_make_empty_symbol		NAME(bfd_elf,make_empty_symbol)
#define elf_get_symbol_info		NAME(bfd_elf,get_symbol_info)
#define elf_get_lineno			NAME(bfd_elf,get_lineno)
#define elf_set_arch_mach		NAME(bfd_elf,set_arch_mach)
#define elf_find_nearest_line		NAME(bfd_elf,find_nearest_line)
#define elf_sizeof_headers		NAME(bfd_elf,sizeof_headers)
#define elf_set_section_contents	NAME(bfd_elf,set_section_contents)
#define elf_no_info_to_howto		NAME(bfd_elf,no_info_to_howto)
#define elf_no_info_to_howto_rel	NAME(bfd_elf,no_info_to_howto_rel)
#define elf_find_section		NAME(bfd_elf,find_section)
#define elf_write_shdrs_and_ehdr	NAME(bfd_elf,write_shdrs_and_ehdr)
#define elf_write_out_phdrs		NAME(bfd_elf,write_out_phdrs)
#define elf_checksum_contents		NAME(bfd_elf,checksum_contents)
#define elf_write_relocs		NAME(bfd_elf,write_relocs)
#define elf_slurp_reloc_table		NAME(bfd_elf,slurp_reloc_table)

#if ARCH_SIZE == 64
#define ELF_R_INFO(X,Y)	ELF64_R_INFO(X,Y)
#define ELF_R_SYM(X)	ELF64_R_SYM(X)
#define ELF_R_TYPE(X)	ELF64_R_TYPE(X)
#define ELFCLASS	ELFCLASS64
#define FILE_ALIGN	8
#define LOG_FILE_ALIGN	3
#endif
#if ARCH_SIZE == 32
#define ELF_R_INFO(X,Y)	ELF32_R_INFO(X,Y)
#define ELF_R_SYM(X)	ELF32_R_SYM(X)
#define ELF_R_TYPE(X)	ELF32_R_TYPE(X)
#define ELFCLASS	ELFCLASS32
#define FILE_ALIGN	4
#define LOG_FILE_ALIGN	2
#endif

#if DEBUG & 2
static void elf_debug_section (int, Elf_Internal_Shdr *);
#endif
#if DEBUG & 1
static void elf_debug_file (Elf_Internal_Ehdr *);
#endif

/* Structure swapping routines */

/* Should perhaps use put_offset, put_word, etc.  For now, the two versions
   can be handled by explicitly specifying 32 bits or "the long type".  */
#if ARCH_SIZE == 64
#define H_PUT_WORD		H_PUT_64
#define H_PUT_SIGNED_WORD	H_PUT_S64
#define H_GET_WORD		H_GET_64
#define H_GET_SIGNED_WORD	H_GET_S64
#endif
#if ARCH_SIZE == 32
#define H_PUT_WORD		H_PUT_32
#define H_PUT_SIGNED_WORD	H_PUT_S32
#define H_GET_WORD		H_GET_32
#define H_GET_SIGNED_WORD	H_GET_S32
#endif

/* Translate an ELF symbol in external format into an ELF symbol in internal
   format.  */

bool
elf_swap_symbol_in (bfd *abfd,
		    const void *psrc,
		    const void *pshn,
		    Elf_Internal_Sym *dst)
{
  const Elf_External_Sym *src = (const Elf_External_Sym *) psrc;
  const Elf_External_Sym_Shndx *shndx = (const Elf_External_Sym_Shndx *) pshn;
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;

  dst->st_name = H_GET_32 (abfd, src->st_name);
  if (signed_vma)
    dst->st_value = H_GET_SIGNED_WORD (abfd, src->st_value);
  else
    dst->st_value = H_GET_WORD (abfd, src->st_value);
  dst->st_size = H_GET_WORD (abfd, src->st_size);
  dst->st_info = H_GET_8 (abfd, src->st_info);
  dst->st_other = H_GET_8 (abfd, src->st_other);
  dst->st_shndx = H_GET_16 (abfd, src->st_shndx);
  if (dst->st_shndx == (SHN_XINDEX & 0xffff))
    {
      if (shndx == NULL)
	return false;
      dst->st_shndx = H_GET_32 (abfd, shndx->est_shndx);
    }
  else if (dst->st_shndx >= (SHN_LORESERVE & 0xffff))
    dst->st_shndx += SHN_LORESERVE - (SHN_LORESERVE & 0xffff);
  dst->st_target_internal = 0;
  return true;
}

/* Translate an ELF symbol in internal format into an ELF symbol in external
   format.  */

void
elf_swap_symbol_out (bfd *abfd,
		     const Elf_Internal_Sym *src,
		     void *cdst,
		     void *shndx)
{
  unsigned int tmp;
  Elf_External_Sym *dst = (Elf_External_Sym *) cdst;
  H_PUT_32 (abfd, src->st_name, dst->st_name);
  H_PUT_WORD (abfd, src->st_value, dst->st_value);
  H_PUT_WORD (abfd, src->st_size, dst->st_size);
  H_PUT_8 (abfd, src->st_info, dst->st_info);
  H_PUT_8 (abfd, src->st_other, dst->st_other);
  tmp = src->st_shndx;
  if (tmp >= (SHN_LORESERVE & 0xffff) && tmp < SHN_LORESERVE)
    {
      if (shndx == NULL)
	abort ();
      H_PUT_32 (abfd, tmp, shndx);
      tmp = SHN_XINDEX & 0xffff;
    }
  H_PUT_16 (abfd, tmp, dst->st_shndx);
}

/* Translate an ELF file header in external format into an ELF file header in
   internal format.  */

static void
elf_swap_ehdr_in (bfd *abfd,
		  const Elf_External_Ehdr *src,
		  Elf_Internal_Ehdr *dst)
{
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;
  memcpy (dst->e_ident, src->e_ident, EI_NIDENT);
  dst->e_type = H_GET_16 (abfd, src->e_type);
  dst->e_machine = H_GET_16 (abfd, src->e_machine);
  dst->e_version = H_GET_32 (abfd, src->e_version);
  if (signed_vma)
    dst->e_entry = H_GET_SIGNED_WORD (abfd, src->e_entry);
  else
    dst->e_entry = H_GET_WORD (abfd, src->e_entry);
  dst->e_phoff = H_GET_WORD (abfd, src->e_phoff);
  dst->e_shoff = H_GET_WORD (abfd, src->e_shoff);
  dst->e_flags = H_GET_32 (abfd, src->e_flags);
  dst->e_ehsize = H_GET_16 (abfd, src->e_ehsize);
  dst->e_phentsize = H_GET_16 (abfd, src->e_phentsize);
  dst->e_phnum = H_GET_16 (abfd, src->e_phnum);
  dst->e_shentsize = H_GET_16 (abfd, src->e_shentsize);
  dst->e_shnum = H_GET_16 (abfd, src->e_shnum);
  dst->e_shstrndx = H_GET_16 (abfd, src->e_shstrndx);
}

/* Translate an ELF file header in internal format into an ELF file header in
   external format.  */

static void
elf_swap_ehdr_out (bfd *abfd,
		   const Elf_Internal_Ehdr *src,
		   Elf_External_Ehdr *dst)
{
  unsigned int tmp;
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;
  bool no_section_header = (abfd->flags & BFD_NO_SECTION_HEADER) != 0;
  memcpy (dst->e_ident, src->e_ident, EI_NIDENT);
  /* note that all elements of dst are *arrays of unsigned char* already...  */
  H_PUT_16 (abfd, src->e_type, dst->e_type);
  H_PUT_16 (abfd, src->e_machine, dst->e_machine);
  H_PUT_32 (abfd, src->e_version, dst->e_version);
  if (signed_vma)
    H_PUT_SIGNED_WORD (abfd, src->e_entry, dst->e_entry);
  else
    H_PUT_WORD (abfd, src->e_entry, dst->e_entry);
  H_PUT_WORD (abfd, src->e_phoff, dst->e_phoff);
  if (no_section_header)
    H_PUT_WORD (abfd, 0, dst->e_shoff);
  else
    H_PUT_WORD (abfd, src->e_shoff, dst->e_shoff);
  H_PUT_32 (abfd, src->e_flags, dst->e_flags);
  H_PUT_16 (abfd, src->e_ehsize, dst->e_ehsize);
  H_PUT_16 (abfd, src->e_phentsize, dst->e_phentsize);
  tmp = src->e_phnum;
  if (tmp > PN_XNUM)
    tmp = PN_XNUM;
  H_PUT_16 (abfd, tmp, dst->e_phnum);
  if (no_section_header)
    {
      H_PUT_16 (abfd, 0, dst->e_shentsize);
      H_PUT_16 (abfd, 0, dst->e_shnum);
      H_PUT_16 (abfd, 0, dst->e_shstrndx);
    }
  else
    {
      H_PUT_16 (abfd, src->e_shentsize, dst->e_shentsize);
      tmp = src->e_shnum;
      if (tmp >= (SHN_LORESERVE & 0xffff))
	tmp = SHN_UNDEF;
      H_PUT_16 (abfd, tmp, dst->e_shnum);
      tmp = src->e_shstrndx;
      if (tmp >= (SHN_LORESERVE & 0xffff))
	tmp = SHN_XINDEX & 0xffff;
      H_PUT_16 (abfd, tmp, dst->e_shstrndx);
    }
}

/* Translate an ELF section header table entry in external format into an
   ELF section header table entry in internal format.  */

static void
elf_swap_shdr_in (bfd *abfd,
		  const Elf_External_Shdr *src,
		  Elf_Internal_Shdr *dst)
{
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;

  dst->sh_name = H_GET_32 (abfd, src->sh_name);
  dst->sh_type = H_GET_32 (abfd, src->sh_type);
  dst->sh_flags = H_GET_WORD (abfd, src->sh_flags);
  if (signed_vma)
    dst->sh_addr = H_GET_SIGNED_WORD (abfd, src->sh_addr);
  else
    dst->sh_addr = H_GET_WORD (abfd, src->sh_addr);
  dst->sh_offset = H_GET_WORD (abfd, src->sh_offset);
  dst->sh_size = H_GET_WORD (abfd, src->sh_size);
  /* PR 23657.  Check for invalid section size, in sections with contents.
     Note - we do not set an error value here because the contents
     of this particular section might not be needed by the consumer.  */
  if (dst->sh_type != SHT_NOBITS)
    {
      ufile_ptr filesize = bfd_get_file_size (abfd);

      if (filesize != 0
	  && ((ufile_ptr) dst->sh_offset > filesize
	      || dst->sh_size > filesize - dst->sh_offset)
	  && !abfd->read_only)
	{
	  _bfd_error_handler (_("warning: %pB has a section "
				"extending past end of file"), abfd);
	  abfd->read_only = 1;
	}
    }
  dst->sh_link = H_GET_32 (abfd, src->sh_link);
  dst->sh_info = H_GET_32 (abfd, src->sh_info);
  dst->sh_addralign = H_GET_WORD (abfd, src->sh_addralign);
  dst->sh_entsize = H_GET_WORD (abfd, src->sh_entsize);
  dst->bfd_section = NULL;
  dst->contents = NULL;
}

/* Translate an ELF section header table entry in internal format into an
   ELF section header table entry in external format.  */

static void
elf_swap_shdr_out (bfd *abfd,
		   const Elf_Internal_Shdr *src,
		   Elf_External_Shdr *dst)
{
  /* note that all elements of dst are *arrays of unsigned char* already...  */
  H_PUT_32 (abfd, src->sh_name, dst->sh_name);
  H_PUT_32 (abfd, src->sh_type, dst->sh_type);
  H_PUT_WORD (abfd, src->sh_flags, dst->sh_flags);
  H_PUT_WORD (abfd, src->sh_addr, dst->sh_addr);
  H_PUT_WORD (abfd, src->sh_offset, dst->sh_offset);
  H_PUT_WORD (abfd, src->sh_size, dst->sh_size);
  H_PUT_32 (abfd, src->sh_link, dst->sh_link);
  H_PUT_32 (abfd, src->sh_info, dst->sh_info);
  H_PUT_WORD (abfd, src->sh_addralign, dst->sh_addralign);
  H_PUT_WORD (abfd, src->sh_entsize, dst->sh_entsize);
}

/* Translate an ELF program header table entry in external format into an
   ELF program header table entry in internal format.  */

void
elf_swap_phdr_in (bfd *abfd,
		  const Elf_External_Phdr *src,
		  Elf_Internal_Phdr *dst)
{
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;

  dst->p_type = H_GET_32 (abfd, src->p_type);
  dst->p_flags = H_GET_32 (abfd, src->p_flags);
  dst->p_offset = H_GET_WORD (abfd, src->p_offset);
  if (signed_vma)
    {
      dst->p_vaddr = H_GET_SIGNED_WORD (abfd, src->p_vaddr);
      dst->p_paddr = H_GET_SIGNED_WORD (abfd, src->p_paddr);
    }
  else
    {
      dst->p_vaddr = H_GET_WORD (abfd, src->p_vaddr);
      dst->p_paddr = H_GET_WORD (abfd, src->p_paddr);
    }
  dst->p_filesz = H_GET_WORD (abfd, src->p_filesz);
  dst->p_memsz = H_GET_WORD (abfd, src->p_memsz);
  dst->p_align = H_GET_WORD (abfd, src->p_align);
}

void
elf_swap_phdr_out (bfd *abfd,
		   const Elf_Internal_Phdr *src,
		   Elf_External_Phdr *dst)
{
  const struct elf_backend_data *bed;
  bfd_vma p_paddr;

  bed = get_elf_backend_data (abfd);
  p_paddr = bed->want_p_paddr_set_to_zero ? 0 : src->p_paddr;

  /* note that all elements of dst are *arrays of unsigned char* already...  */
  H_PUT_32 (abfd, src->p_type, dst->p_type);
  H_PUT_WORD (abfd, src->p_offset, dst->p_offset);
  H_PUT_WORD (abfd, src->p_vaddr, dst->p_vaddr);
  H_PUT_WORD (abfd, p_paddr, dst->p_paddr);
  H_PUT_WORD (abfd, src->p_filesz, dst->p_filesz);
  H_PUT_WORD (abfd, src->p_memsz, dst->p_memsz);
  H_PUT_32 (abfd, src->p_flags, dst->p_flags);
  H_PUT_WORD (abfd, src->p_align, dst->p_align);
}

/* Translate an ELF reloc from external format to internal format.  */
void
elf_swap_reloc_in (bfd *abfd,
		   const bfd_byte *s,
		   Elf_Internal_Rela *dst)
{
  const Elf_External_Rel *src = (const Elf_External_Rel *) s;
  dst->r_offset = H_GET_WORD (abfd, src->r_offset);
  dst->r_info = H_GET_WORD (abfd, src->r_info);
  dst->r_addend = 0;
}

void
elf_swap_reloca_in (bfd *abfd,
		    const bfd_byte *s,
		    Elf_Internal_Rela *dst)
{
  const Elf_External_Rela *src = (const Elf_External_Rela *) s;
  dst->r_offset = H_GET_WORD (abfd, src->r_offset);
  dst->r_info = H_GET_WORD (abfd, src->r_info);
  dst->r_addend = H_GET_SIGNED_WORD (abfd, src->r_addend);
}

/* Translate an ELF reloc from internal format to external format.  */
void
elf_swap_reloc_out (bfd *abfd,
		    const Elf_Internal_Rela *src,
		    bfd_byte *d)
{
  Elf_External_Rel *dst = (Elf_External_Rel *) d;
  H_PUT_WORD (abfd, src->r_offset, dst->r_offset);
  H_PUT_WORD (abfd, src->r_info, dst->r_info);
}

void
elf_swap_reloca_out (bfd *abfd,
		     const Elf_Internal_Rela *src,
		     bfd_byte *d)
{
  Elf_External_Rela *dst = (Elf_External_Rela *) d;
  H_PUT_WORD (abfd, src->r_offset, dst->r_offset);
  H_PUT_WORD (abfd, src->r_info, dst->r_info);
  H_PUT_SIGNED_WORD (abfd, src->r_addend, dst->r_addend);
}

void
elf_swap_dyn_in (bfd *abfd,
		 const void *p,
		 Elf_Internal_Dyn *dst)
{
  const Elf_External_Dyn *src = (const Elf_External_Dyn *) p;

  dst->d_tag = H_GET_WORD (abfd, src->d_tag);
  dst->d_un.d_val = H_GET_WORD (abfd, src->d_un.d_val);
}

void
elf_swap_dyn_out (bfd *abfd,
		  const Elf_Internal_Dyn *src,
		  void *p)
{
  Elf_External_Dyn *dst = (Elf_External_Dyn *) p;

  H_PUT_WORD (abfd, src->d_tag, dst->d_tag);
  H_PUT_WORD (abfd, src->d_un.d_val, dst->d_un.d_val);
}

/* ELF .o/exec file reading */

/* Begin processing a given object.

   First we validate the file by reading in the ELF header and checking
   the magic number.  */

static inline bool
elf_file_p (Elf_External_Ehdr *x_ehdrp)
{
  return ((x_ehdrp->e_ident[EI_MAG0] == ELFMAG0)
	  && (x_ehdrp->e_ident[EI_MAG1] == ELFMAG1)
	  && (x_ehdrp->e_ident[EI_MAG2] == ELFMAG2)
	  && (x_ehdrp->e_ident[EI_MAG3] == ELFMAG3));
}

/* Check to see if the file associated with ABFD matches the target vector
   that ABFD points to.

   Note that we may be called several times with the same ABFD, but different
   target vectors, most of which will not match.  We have to avoid leaving
   any side effects in ABFD, or any data it points to (like tdata), if the
   file does not match the target vector.  */

bfd_cleanup
elf_object_p (bfd *abfd)
{
  Elf_External_Ehdr x_ehdr;	/* Elf file header, external form */
  Elf_Internal_Ehdr *i_ehdrp;	/* Elf file header, internal form */
  Elf_External_Shdr x_shdr;	/* Section header table entry, external form */
  Elf_Internal_Shdr i_shdr;
  Elf_Internal_Shdr *i_shdrp;	/* Section header table, internal form */
  unsigned int shindex;
  const struct elf_backend_data *ebd;
  asection *s;
  const bfd_target *target;

  /* Read in the ELF header in external format.  */

  if (bfd_read (&x_ehdr, sizeof (x_ehdr), abfd) != sizeof (x_ehdr))
    {
      if (bfd_get_error () != bfd_error_system_call)
	goto got_wrong_format_error;
      else
	goto got_no_match;
    }

  /* Now check to see if we have a valid ELF file, and one that BFD can
     make use of.  The magic number must match, the address size ('class')
     and byte-swapping must match our XVEC entry, and it must have a
     section header table (FIXME: See comments re sections at top of this
     file).  */

  if (! elf_file_p (&x_ehdr)
      || x_ehdr.e_ident[EI_VERSION] != EV_CURRENT
      || x_ehdr.e_ident[EI_CLASS] != ELFCLASS)
    goto got_wrong_format_error;

  /* Check that file's byte order matches xvec's */
  switch (x_ehdr.e_ident[EI_DATA])
    {
    case ELFDATA2MSB:		/* Big-endian */
      if (! bfd_header_big_endian (abfd))
	goto got_wrong_format_error;
      break;
    case ELFDATA2LSB:		/* Little-endian */
      if (! bfd_header_little_endian (abfd))
	goto got_wrong_format_error;
      break;
    case ELFDATANONE:		/* No data encoding specified */
    default:			/* Unknown data encoding specified */
      goto got_wrong_format_error;
    }

  target = abfd->xvec;

  /* Allocate an instance of the elf_obj_tdata structure and hook it up to
     the tdata pointer in the bfd.  */

  if (! (*target->_bfd_set_format[bfd_object]) (abfd))
    goto got_no_match;

  /* Now that we know the byte order, swap in the rest of the header */
  i_ehdrp = elf_elfheader (abfd);
  elf_swap_ehdr_in (abfd, &x_ehdr, i_ehdrp);
#if DEBUG & 1
  elf_debug_file (i_ehdrp);
#endif

  /* Reject ET_CORE (header indicates core file, not object file) */
  if (i_ehdrp->e_type == ET_CORE)
    goto got_wrong_format_error;

  /* If this is a relocatable file and there is no section header
     table, then we're hosed.  */
  if (i_ehdrp->e_shoff < sizeof (x_ehdr) && i_ehdrp->e_type == ET_REL)
    goto got_wrong_format_error;

  /* As a simple sanity check, verify that what BFD thinks is the
     size of each section header table entry actually matches the size
     recorded in the file, but only if there are any sections.  */
  if (i_ehdrp->e_shentsize != sizeof (x_shdr) && i_ehdrp->e_shnum != 0)
    goto got_wrong_format_error;

  /* Further sanity check.  */
  if (i_ehdrp->e_shoff < sizeof (x_ehdr) && i_ehdrp->e_shnum != 0)
    goto got_wrong_format_error;

  ebd = get_elf_backend_data (abfd);
  if (ebd->s->arch_size != ARCH_SIZE)
    goto got_wrong_format_error;

  /* Check that the ELF e_machine field matches what this particular
     BFD format expects.  */
  if (ebd->elf_machine_code != i_ehdrp->e_machine
      && (ebd->elf_machine_alt1 == 0
	  || i_ehdrp->e_machine != ebd->elf_machine_alt1)
      && (ebd->elf_machine_alt2 == 0
	  || i_ehdrp->e_machine != ebd->elf_machine_alt2)
      && ebd->elf_machine_code != EM_NONE)
    goto got_wrong_format_error;

  if (i_ehdrp->e_type == ET_EXEC)
    abfd->flags |= EXEC_P;
  else if (i_ehdrp->e_type == ET_DYN)
    abfd->flags |= DYNAMIC;

  if (i_ehdrp->e_phnum > 0)
    abfd->flags |= D_PAGED;

  if (! bfd_default_set_arch_mach (abfd, ebd->arch, 0))
    {
      /* It's OK if this fails for the generic target.  */
      if (ebd->elf_machine_code != EM_NONE)
	goto got_no_match;
    }

  if (ebd->elf_machine_code != EM_NONE
      && i_ehdrp->e_ident[EI_OSABI] != ebd->elf_osabi
      && ebd->elf_osabi != ELFOSABI_NONE)
    goto got_wrong_format_error;

  if (i_ehdrp->e_shoff >= sizeof (x_ehdr))
    {
      file_ptr where = (file_ptr) i_ehdrp->e_shoff;

      /* Seek to the section header table in the file.  */
      if (bfd_seek (abfd, where, SEEK_SET) != 0)
	goto got_no_match;

      /* Read the first section header at index 0, and convert to internal
	 form.  */
      if (bfd_read (&x_shdr, sizeof x_shdr, abfd) != sizeof (x_shdr))
	goto got_no_match;
      elf_swap_shdr_in (abfd, &x_shdr, &i_shdr);

      /* If the section count is zero, the actual count is in the first
	 section header.  */
      if (i_ehdrp->e_shnum == SHN_UNDEF)
	{
	  i_ehdrp->e_shnum = i_shdr.sh_size;
	  if (i_ehdrp->e_shnum >= SHN_LORESERVE
	      || i_ehdrp->e_shnum != i_shdr.sh_size
	      || i_ehdrp->e_shnum  == 0)
	    goto got_wrong_format_error;
	}

      /* And similarly for the string table index.  */
      if (i_ehdrp->e_shstrndx == (SHN_XINDEX & 0xffff))
	{
	  i_ehdrp->e_shstrndx = i_shdr.sh_link;
	  if (i_ehdrp->e_shstrndx != i_shdr.sh_link)
	    goto got_wrong_format_error;
	}

      /* And program headers.  */
      if (i_ehdrp->e_phnum == PN_XNUM && i_shdr.sh_info != 0)
	{
	  i_ehdrp->e_phnum = i_shdr.sh_info;
	  if (i_ehdrp->e_phnum != i_shdr.sh_info)
	    goto got_wrong_format_error;
	}

      /* Sanity check that we can read all of the section headers.
	 It ought to be good enough to just read the last one.  */
      if (i_ehdrp->e_shnum != 1)
	{
	  /* Check that we don't have a totally silly number of sections.  */
	  if (i_ehdrp->e_shnum > (unsigned int) -1 / sizeof (x_shdr)
	      || i_ehdrp->e_shnum > (unsigned int) -1 / sizeof (i_shdr))
	    goto got_wrong_format_error;

	  where += (i_ehdrp->e_shnum - 1) * sizeof (x_shdr);
	  if ((bfd_size_type) where <= i_ehdrp->e_shoff)
	    goto got_wrong_format_error;

	  if (bfd_seek (abfd, where, SEEK_SET) != 0)
	    goto got_no_match;
	  if (bfd_read (&x_shdr, sizeof x_shdr, abfd) != sizeof (x_shdr))
	    goto got_no_match;

	  /* Back to where we were.  */
	  where = i_ehdrp->e_shoff + sizeof (x_shdr);
	  if (bfd_seek (abfd, where, SEEK_SET) != 0)
	    goto got_no_match;
	}
    }

  /* Allocate space for a copy of the section header table in
     internal form.  */
  if (i_ehdrp->e_shnum != 0)
    {
      Elf_Internal_Shdr *shdrp;
      unsigned int num_sec;
      size_t amt;

      if (_bfd_mul_overflow (i_ehdrp->e_shnum, sizeof (*i_shdrp), &amt))
	goto got_wrong_format_error;
      i_shdrp = (Elf_Internal_Shdr *) bfd_alloc (abfd, amt);
      if (!i_shdrp)
	goto got_no_match;
      num_sec = i_ehdrp->e_shnum;
      elf_numsections (abfd) = num_sec;
      if (_bfd_mul_overflow (num_sec, sizeof (i_shdrp), &amt))
	goto got_wrong_format_error;
      elf_elfsections (abfd) = (Elf_Internal_Shdr **) bfd_alloc (abfd, amt);
      if (!elf_elfsections (abfd))
	goto got_no_match;
      elf_tdata (abfd)->being_created = bfd_zalloc (abfd, num_sec);
      if (!elf_tdata (abfd)->being_created)
	goto got_no_match;

      memcpy (i_shdrp, &i_shdr, sizeof (*i_shdrp));
      for (shdrp = i_shdrp, shindex = 0; shindex < num_sec; shindex++)
	elf_elfsections (abfd)[shindex] = shdrp++;

      /* Read in the rest of the section header table and convert it
	 to internal form.  */
      for (shindex = 1; shindex < i_ehdrp->e_shnum; shindex++)
	{
	  if (bfd_read (&x_shdr, sizeof x_shdr, abfd) != sizeof (x_shdr))
	    goto got_no_match;
	  elf_swap_shdr_in (abfd, &x_shdr, i_shdrp + shindex);

	  /* Sanity check sh_link and sh_info.  */
	  if (i_shdrp[shindex].sh_link >= num_sec)
	    {
	      /* PR 10478: Accept Solaris binaries with a sh_link
		 field set to SHN_BEFORE or SHN_AFTER.  */
	      switch (ebd->elf_machine_code)
		{
		case EM_386:
		case EM_IAMCU:
		case EM_X86_64:
		case EM_OLD_SPARCV9:
		case EM_SPARC32PLUS:
		case EM_SPARCV9:
		case EM_SPARC:
		  if (i_shdrp[shindex].sh_link == (SHN_LORESERVE & 0xffff) /* SHN_BEFORE */
		      || i_shdrp[shindex].sh_link == ((SHN_LORESERVE + 1) & 0xffff) /* SHN_AFTER */)
		    break;
		  /* Otherwise fall through.  */
		default:
		  goto got_wrong_format_error;
		}
	    }

	  if (((i_shdrp[shindex].sh_flags & SHF_INFO_LINK)
	       || i_shdrp[shindex].sh_type == SHT_RELA
	       || i_shdrp[shindex].sh_type == SHT_REL)
	      && i_shdrp[shindex].sh_info >= num_sec)
	    goto got_wrong_format_error;

	  /* If the section is loaded, but not page aligned, clear
	     D_PAGED.  */
	  if (i_shdrp[shindex].sh_size != 0
	      && (i_shdrp[shindex].sh_flags & SHF_ALLOC) != 0
	      && i_shdrp[shindex].sh_type != SHT_NOBITS
	      && (((i_shdrp[shindex].sh_addr - i_shdrp[shindex].sh_offset)
		   % ebd->minpagesize)
		  != 0))
	    abfd->flags &= ~D_PAGED;
	}

      if (i_ehdrp->e_shstrndx >= elf_numsections (abfd)
	  || i_shdrp[i_ehdrp->e_shstrndx].sh_type != SHT_STRTAB)
	{
	  /* PR 2257:
	     We used to just goto got_wrong_format_error here
	     but there are binaries in existance for which this test
	     will prevent the binutils from working with them at all.
	     So we are kind, and reset the string index value to 0
	     so that at least some processing can be done.  */
	  i_ehdrp->e_shstrndx = SHN_UNDEF;
	  if (!abfd->read_only)
	    {
	      _bfd_error_handler
		(_("warning: %pB has a corrupt string table index"), abfd);
	      abfd->read_only = 1;
	    }
	}
    }
  else if (i_ehdrp->e_shstrndx != SHN_UNDEF)
    goto got_wrong_format_error;

  /* Read in the program headers.  */
  if (i_ehdrp->e_phnum == 0)
    elf_tdata (abfd)->phdr = NULL;
  else
    {
      Elf_Internal_Phdr *i_phdr;
      unsigned int i;
      ufile_ptr filesize;
      size_t amt;

      /* Check for a corrupt input file with an impossibly large number
	 of program headers.  */
      filesize = bfd_get_file_size (abfd);
      if (filesize != 0
	  && i_ehdrp->e_phnum > filesize / sizeof (Elf_External_Phdr))
	goto got_wrong_format_error;
      if (_bfd_mul_overflow (i_ehdrp->e_phnum, sizeof (*i_phdr), &amt))
	goto got_wrong_format_error;
      elf_tdata (abfd)->phdr
	= (Elf_Internal_Phdr *) bfd_alloc (abfd, amt);
      if (elf_tdata (abfd)->phdr == NULL)
	goto got_no_match;
      if (bfd_seek (abfd, i_ehdrp->e_phoff, SEEK_SET) != 0)
	goto got_no_match;
      bool eu_strip_broken_phdrs = false;
      i_phdr = elf_tdata (abfd)->phdr;
      for (i = 0; i < i_ehdrp->e_phnum; i++, i_phdr++)
	{
	  Elf_External_Phdr x_phdr;

	  if (bfd_read (&x_phdr, sizeof x_phdr, abfd) != sizeof x_phdr)
	    goto got_no_match;
	  elf_swap_phdr_in (abfd, &x_phdr, i_phdr);
	  /* Too much code in BFD relies on alignment being a power of
	     two, as required by the ELF spec.  */
	  if (i_phdr->p_align != (i_phdr->p_align & -i_phdr->p_align))
	    {
	      i_phdr->p_align &= -i_phdr->p_align;
	      if (!abfd->read_only)
		{
		  _bfd_error_handler (_("warning: %pB has a program header "
					"with invalid alignment"), abfd);
		  abfd->read_only = 1;
		}
	    }
	  /* Detect eu-strip -f debug files, which have program
	     headers that describe the original file.  */
	  if (i_phdr->p_filesz != 0
	      && (i_phdr->p_filesz > filesize
		  || i_phdr->p_offset > filesize - i_phdr->p_filesz))
	    eu_strip_broken_phdrs = true;
	}
      if (!eu_strip_broken_phdrs
	  && i_ehdrp->e_shoff == 0
	  && i_ehdrp->e_shstrndx == 0)
	{
	  /* Try to reconstruct dynamic symbol table from PT_DYNAMIC
	     segment if there is no section header.  */
	  i_phdr = elf_tdata (abfd)->phdr;
	  for (i = 0; i < i_ehdrp->e_phnum; i++, i_phdr++)
	    if (i_phdr->p_type == PT_DYNAMIC)
	      {
		if (i_phdr->p_filesz != 0
		    && !_bfd_elf_get_dynamic_symbols (abfd, i_phdr,
						      elf_tdata (abfd)->phdr,
						      i_ehdrp->e_phnum,
						      filesize))
		  goto got_no_match;
		break;
	      }
	}
    }

  if (i_ehdrp->e_shstrndx != 0 && i_ehdrp->e_shoff >= sizeof (x_ehdr))
    {
      unsigned int num_sec;

      /* Once all of the section headers have been read and converted, we
	 can start processing them.  Note that the first section header is
	 a dummy placeholder entry, so we ignore it.  */
      num_sec = elf_numsections (abfd);
      for (shindex = 1; shindex < num_sec; shindex++)
	if (!bfd_section_from_shdr (abfd, shindex))
	  goto got_no_match;

      /* Set up ELF sections for SHF_GROUP and SHF_LINK_ORDER.  */
      if (! _bfd_elf_setup_sections (abfd))
	goto got_wrong_format_error;
    }

  /* Let the backend double check the format and override global
     information.  */
  if (ebd->elf_backend_object_p)
    {
      if (! (*ebd->elf_backend_object_p) (abfd))
	goto got_wrong_format_error;
    }

  /* Remember the entry point specified in the ELF file header.  */
  bfd_set_start_address (abfd, i_ehdrp->e_entry);

  /* If we have created any reloc sections that are associated with
     debugging sections, mark the reloc sections as debugging as well.  */
  for (s = abfd->sections; s != NULL; s = s->next)
    {
      if ((elf_section_data (s)->this_hdr.sh_type == SHT_REL
	   || elf_section_data (s)->this_hdr.sh_type == SHT_RELA)
	  && elf_section_data (s)->this_hdr.sh_info > 0)
	{
	  unsigned long targ_index;
	  asection *targ_sec;

	  targ_index = elf_section_data (s)->this_hdr.sh_info;
	  targ_sec = bfd_section_from_elf_index (abfd, targ_index);
	  if (targ_sec != NULL
	      && (targ_sec->flags & SEC_DEBUGGING) != 0)
	    s->flags |= SEC_DEBUGGING;
	}
    }
  return _bfd_no_cleanup;

 got_wrong_format_error:
  bfd_set_error (bfd_error_wrong_format);

 got_no_match:
  return NULL;
}

/* ELF .o/exec file writing */

/* Write out the relocs.  */

void
elf_write_relocs (bfd *abfd, asection *sec, void *data)
{
  const struct elf_backend_data * const bed = get_elf_backend_data (abfd);
  bool *failedp = (bool *) data;
  Elf_Internal_Shdr *rela_hdr;
  bfd_vma addr_offset;
  void (*swap_out) (bfd *, const Elf_Internal_Rela *, bfd_byte *);
  size_t extsize;
  bfd_byte *dst_rela;
  unsigned int idx;
  asymbol *last_sym;
  int last_sym_idx;
  size_t amt;

  /* If we have already failed, don't do anything.  */
  if (*failedp)
    return;

  if ((sec->flags & SEC_RELOC) == 0)
    return;

  /* The linker backend writes the relocs out itself, and sets the
     reloc_count field to zero to inhibit writing them here.  Also,
     sometimes the SEC_RELOC flag gets set even when there aren't any
     relocs.  */
  if (sec->reloc_count == 0)
    return;

  /* If we have opened an existing file for update, reloc_count may be
     set even though we are not linking.  In that case we have nothing
     to do.  */
  if (sec->orelocation == NULL)
    return;

  rela_hdr = elf_section_data (sec)->rela.hdr;
  if (rela_hdr == NULL)
    rela_hdr = elf_section_data (sec)->rel.hdr;

  rela_hdr->sh_size = rela_hdr->sh_entsize * sec->reloc_count;
  if (_bfd_mul_overflow (sec->reloc_count, rela_hdr->sh_entsize, &amt)
      || (rela_hdr->contents = bfd_alloc (abfd, amt)) == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      *failedp = true;
      return;
    }

  /* Figure out whether the relocations are RELA or REL relocations.  */
  if (rela_hdr->sh_type == SHT_RELA)
    {
      swap_out = elf_swap_reloca_out;
      extsize = sizeof (Elf_External_Rela);
    }
  else if (rela_hdr->sh_type == SHT_REL)
    {
      swap_out = elf_swap_reloc_out;
      extsize = sizeof (Elf_External_Rel);
    }
  else
    /* Every relocation section should be either an SHT_RELA or an
       SHT_REL section.  */
    abort ();

  /* The address of an ELF reloc is section relative for an object
     file, and absolute for an executable file or shared library.
     The address of a BFD reloc is always section relative.  */
  addr_offset = 0;
  if ((abfd->flags & (EXEC_P | DYNAMIC)) != 0)
    addr_offset = sec->vma;

  /* orelocation has the data, reloc_count has the count...  */
  last_sym = 0;
  last_sym_idx = 0;
  dst_rela = rela_hdr->contents;

  for (idx = 0; idx < sec->reloc_count; idx++, dst_rela += extsize)
    {
      Elf_Internal_Rela src_rela;
      arelent *ptr;
      asymbol *sym;
      int n;

      ptr = sec->orelocation[idx];
      sym = *ptr->sym_ptr_ptr;
      if (sym == last_sym)
	n = last_sym_idx;
      /* If the relocation is against an absolute symbol whoes value is
	 zero, then the symbol can be dropped, simplifying the reloc.
	 PR 31106: Except for complex relocations where the symbols
	 itself might be significant.  */
      else if (bfd_is_abs_section (sym->section)
	       && sym->value == 0
	       && (sym->flags & BSF_RELC) == 0)
	n = STN_UNDEF;
      else
	{
	  last_sym = sym;
	  n = _bfd_elf_symbol_from_bfd_symbol (abfd, &sym);
	  if (n < 0)
	    {
	      *failedp = true;
	      return;
	    }
	  last_sym_idx = n;
	}

      if ((*ptr->sym_ptr_ptr)->the_bfd != NULL
	  && (*ptr->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec
	  && ! _bfd_elf_validate_reloc (abfd, ptr))
	{
	  *failedp = true;
	  return;
	}

      if (ptr->howto == NULL)
	{
	  *failedp = true;
	  return;
	}

#if defined(BFD64) && ARCH_SIZE == 32
      if (rela_hdr->sh_type == SHT_RELA
	  && ptr->howto->bitsize > 32
	  && ptr->addend - INT32_MIN > UINT32_MAX)
	{
	  _bfd_error_handler (_("%pB: %pA+%" PRIx64 ": "
				"relocation addend %" PRIx64 " too large"),
			      abfd, sec, (uint64_t) ptr->address,
			      (uint64_t) ptr->addend);
	  *failedp = true;
	  bfd_set_error (bfd_error_bad_value);
	}
#endif

      src_rela.r_offset = ptr->address + addr_offset;
      src_rela.r_info = ELF_R_INFO (n, ptr->howto->type);
      src_rela.r_addend = ptr->addend;
      (*swap_out) (abfd, &src_rela, dst_rela);
    }

  if (elf_section_data (sec)->has_secondary_relocs
      && !bed->write_secondary_relocs (abfd, sec))
    {
      *failedp = true;
      return;
    }
}

/* Write out the program headers.  */

int
elf_write_out_phdrs (bfd *abfd,
		     const Elf_Internal_Phdr *phdr,
		     unsigned int count)
{
  while (count--)
    {
      Elf_External_Phdr extphdr;

      elf_swap_phdr_out (abfd, phdr, &extphdr);
      if (bfd_write (&extphdr, sizeof (Elf_External_Phdr), abfd)
	  != sizeof (Elf_External_Phdr))
	return -1;
      phdr++;
    }
  return 0;
}

/* Write out the section headers and the ELF file header.  */

bool
elf_write_shdrs_and_ehdr (bfd *abfd)
{
  Elf_External_Ehdr x_ehdr;	/* Elf file header, external form */
  Elf_Internal_Ehdr *i_ehdrp;	/* Elf file header, internal form */
  Elf_External_Shdr *x_shdrp;	/* Section header table, external form */
  Elf_Internal_Shdr **i_shdrp;	/* Section header table, internal form */
  unsigned int count;
  size_t amt;

  i_ehdrp = elf_elfheader (abfd);
  i_shdrp = elf_elfsections (abfd);

  /* swap the header before spitting it out...  */

#if DEBUG & 1
  elf_debug_file (i_ehdrp);
#endif
  elf_swap_ehdr_out (abfd, i_ehdrp, &x_ehdr);
  amt = sizeof (x_ehdr);
  if (bfd_seek (abfd, 0, SEEK_SET) != 0
      || bfd_write (&x_ehdr, amt, abfd) != amt)
    return false;

  if ((abfd->flags & BFD_NO_SECTION_HEADER) != 0)
    return true;

  /* Some fields in the first section header handle overflow of ehdr
     fields.  */
  if (i_ehdrp->e_phnum >= PN_XNUM)
    i_shdrp[0]->sh_info = i_ehdrp->e_phnum;
  if (i_ehdrp->e_shnum >= (SHN_LORESERVE & 0xffff))
    i_shdrp[0]->sh_size = i_ehdrp->e_shnum;
  if (i_ehdrp->e_shstrndx >= (SHN_LORESERVE & 0xffff))
    i_shdrp[0]->sh_link = i_ehdrp->e_shstrndx;

  /* at this point we've concocted all the ELF sections...  */
  if (_bfd_mul_overflow (i_ehdrp->e_shnum, sizeof (*x_shdrp), &amt))
    {
      bfd_set_error (bfd_error_no_memory);
      return false;
    }
  x_shdrp = (Elf_External_Shdr *) bfd_alloc (abfd, amt);
  if (!x_shdrp)
    return false;

  for (count = 0; count < i_ehdrp->e_shnum; i_shdrp++, count++)
    {
#if DEBUG & 2
      elf_debug_section (count, *i_shdrp);
#endif
      elf_swap_shdr_out (abfd, *i_shdrp, x_shdrp + count);
    }
  amt = (bfd_size_type) i_ehdrp->e_shnum * sizeof (*x_shdrp);
  if (bfd_seek (abfd, i_ehdrp->e_shoff, SEEK_SET) != 0
      || bfd_write (x_shdrp, amt, abfd) != amt)
    return false;

  /* need to dump the string table too...  */

  return true;
}

bool
elf_checksum_contents (bfd *abfd,
		       void (*process) (const void *, size_t, void *),
		       void *arg)
{
  Elf_Internal_Ehdr *i_ehdrp = elf_elfheader (abfd);
  Elf_Internal_Shdr **i_shdrp = elf_elfsections (abfd);
  Elf_Internal_Phdr *i_phdrp = elf_tdata (abfd)->phdr;
  unsigned int count, num;

  {
    Elf_External_Ehdr x_ehdr;
    Elf_Internal_Ehdr i_ehdr;

    i_ehdr = *i_ehdrp;
    i_ehdr.e_phoff = i_ehdr.e_shoff = 0;
    elf_swap_ehdr_out (abfd, &i_ehdr, &x_ehdr);
    (*process) (&x_ehdr, sizeof x_ehdr, arg);
  }

  num = i_ehdrp->e_phnum;
  for (count = 0; count < num; count++)
    {
      Elf_External_Phdr x_phdr;
      elf_swap_phdr_out (abfd, &i_phdrp[count], &x_phdr);
      (*process) (&x_phdr, sizeof x_phdr, arg);
    }

  num = elf_numsections (abfd);
  for (count = 0; count < num; count++)
    {
      Elf_Internal_Shdr i_shdr;
      Elf_External_Shdr x_shdr;
      bfd_byte *contents, *free_contents;
      asection *sec = NULL;

      i_shdr = *i_shdrp[count];
      i_shdr.sh_offset = 0;

      elf_swap_shdr_out (abfd, &i_shdr, &x_shdr);
      (*process) (&x_shdr, sizeof x_shdr, arg);

      /* Process the section's contents, if it has some.
	 PR ld/12451: Read them in if necessary.  */
      if (i_shdr.sh_type == SHT_NOBITS)
	continue;
      free_contents = NULL;
      contents = i_shdr.contents;
      if (contents == NULL)
	{
	  sec = bfd_section_from_elf_index (abfd, count);
	  if (sec != NULL)
	    {
	      contents = sec->contents;
	      if (contents == NULL)
		{
		  /* Force rereading from file.  */
		  sec->flags &= ~SEC_IN_MEMORY;
		  if (!_bfd_elf_mmap_section_contents (abfd, sec, &free_contents))
		    continue;
		  contents = free_contents;
		}
	    }
	}
      if (contents != NULL)
	{
	  (*process) (contents, i_shdr.sh_size, arg);
	  _bfd_elf_munmap_section_contents (sec, free_contents);
	}
    }

  return true;
}

long
elf_slurp_symbol_table (bfd *abfd, asymbol **symptrs, bool dynamic)
{
  Elf_Internal_Shdr *hdr;
  Elf_Internal_Shdr *verhdr;
  unsigned long symcount;	/* Number of external ELF symbols */
  elf_symbol_type *sym;		/* Pointer to current bfd symbol */
  elf_symbol_type *symbase;	/* Buffer for generated bfd symbols */
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymend;
  Elf_Internal_Sym *isymbuf = NULL;
  Elf_External_Versym *xver;
  Elf_External_Versym *xverbuf = NULL;
  const struct elf_backend_data *ebd;
  size_t amt;

  /* Read each raw ELF symbol, converting from external ELF form to
     internal ELF form, and then using the information to create a
     canonical bfd symbol table entry.

     Note that we allocate the initial bfd canonical symbol buffer
     based on a one-to-one mapping of the ELF symbols to canonical
     symbols.  We actually use all the ELF symbols, so there will be no
     space left over at the end.  When we have all the symbols, we
     build the caller's pointer vector.  */
  ebd = get_elf_backend_data (abfd);

  if (! dynamic)
    {
      hdr = &elf_tdata (abfd)->symtab_hdr;
      verhdr = NULL;
      symcount = hdr->sh_size / ebd->s->sizeof_sym;
    }
  else
    {
      hdr = &elf_tdata (abfd)->dynsymtab_hdr;
      if (elf_dynversym (abfd) == 0)
	verhdr = NULL;
      else
	verhdr = &elf_tdata (abfd)->dynversym_hdr;
      if ((elf_dynverdef (abfd) != 0
	   && elf_tdata (abfd)->verdef == NULL)
	  || (elf_dynverref (abfd) != 0
	      && elf_tdata (abfd)->verref == NULL)
	  || elf_tdata (abfd)->dt_verdef != NULL
	  || elf_tdata (abfd)->dt_verneed != NULL)
	{
	  if (!_bfd_elf_slurp_version_tables (abfd, false))
	    return -1;
	}

      symcount = elf_tdata (abfd)->dt_symtab_count;
    }

  if (symcount == 0)
    symcount = hdr->sh_size / sizeof (Elf_External_Sym);

  if (symcount == 0)
    sym = symbase = NULL;
  else
    {
      size_t i;

      isymbuf = bfd_elf_get_elf_syms (abfd, hdr, symcount, 0,
				      NULL, NULL, NULL);
      if (isymbuf == NULL)
	return -1;

      if (_bfd_mul_overflow (symcount, sizeof (elf_symbol_type), &amt))
	{
	  bfd_set_error (bfd_error_file_too_big);
	  goto error_return;
	}
      symbase = (elf_symbol_type *) bfd_zalloc (abfd, amt);
      if (symbase == (elf_symbol_type *) NULL)
	goto error_return;

      /* Read the raw ELF version symbol information.  */
      if (verhdr != NULL
	  && verhdr->sh_size / sizeof (Elf_External_Versym) != symcount)
	{
	  _bfd_error_handler
	    /* xgettext:c-format */
	    (_("%pB: version count (%" PRId64 ")"
	       " does not match symbol count (%ld)"),
	     abfd,
	     (int64_t) (verhdr->sh_size / sizeof (Elf_External_Versym)),
	     symcount);

	  /* Slurp in the symbols without the version information,
	     since that is more helpful than just quitting.  */
	  verhdr = NULL;
	}

      if (verhdr != NULL)
	{
	  if (bfd_seek (abfd, verhdr->sh_offset, SEEK_SET) != 0)
	    goto error_return;
	  xverbuf = (Elf_External_Versym *)
	    _bfd_malloc_and_read (abfd, verhdr->sh_size, verhdr->sh_size);
	  if (xverbuf == NULL && verhdr->sh_size != 0)
	    goto error_return;
	}

      /* Skip first symbol, which is a null dummy.  */
      xver = xverbuf;
      if (xver != NULL)
	++xver;
      isymend = isymbuf + symcount;
      for (isym = isymbuf + 1, sym = symbase, i = 1;
	   isym < isymend;
	   isym++, sym++, i++)
	{
	  memcpy (&sym->internal_elf_sym, isym, sizeof (Elf_Internal_Sym));

	  sym->symbol.the_bfd = abfd;
	  if (elf_use_dt_symtab_p (abfd))
	    sym->symbol.name = (elf_tdata (abfd)->dt_strtab
				+ isym->st_name);
	  else
	    sym->symbol.name = bfd_elf_sym_name (abfd, hdr, isym, NULL);
	  sym->symbol.value = isym->st_value;

	  if (isym->st_shndx == SHN_UNDEF)
	    {
	      sym->symbol.section = bfd_und_section_ptr;
	    }
	  else if (isym->st_shndx == SHN_ABS)
	    {
	      sym->symbol.section = bfd_abs_section_ptr;
	    }
	  else if (isym->st_shndx == SHN_COMMON)
	    {
	      sym->symbol.section = bfd_com_section_ptr;
	      if ((abfd->flags & BFD_PLUGIN) != 0)
		{
		  asection *xc = bfd_get_section_by_name (abfd, "COMMON");

		  if (xc == NULL)
		    {
		      flagword flags = (SEC_ALLOC | SEC_IS_COMMON | SEC_KEEP
					| SEC_EXCLUDE);
		      xc = bfd_make_section_with_flags (abfd, "COMMON", flags);
		      if (xc == NULL)
			goto error_return;
		    }
		  sym->symbol.section = xc;
		}
	      /* Elf puts the alignment into the `value' field, and
		 the size into the `size' field.  BFD wants to see the
		 size in the value field, and doesn't care (at the
		 moment) about the alignment.  */
	      sym->symbol.value = isym->st_size;
	    }
	  else if (elf_use_dt_symtab_p (abfd))
	    {
	      asection *sec;
	      sec = _bfd_elf_get_section_from_dynamic_symbol (abfd,
							      isym);
	      if (sec == NULL)
		goto error_return;
	      sym->symbol.section = sec;
	    }
	  else
	    {
	      sym->symbol.section
		= bfd_section_from_elf_index (abfd, isym->st_shndx);
	      if (sym->symbol.section == NULL)
		{
		  /* This symbol is in a section for which we did not
		     create a BFD section.  Just use bfd_abs_section,
		     although it is wrong.  FIXME.  Note - there is
		     code in elf.c:swap_out_syms that calls
		     symbol_section_index() in the elf backend for
		     cases like this.  */
		  sym->symbol.section = bfd_abs_section_ptr;
		}
	    }

	  /* If this is a relocatable file, then the symbol value is
	     already section relative.  */
	  if ((abfd->flags & (EXEC_P | DYNAMIC)) != 0)
	    sym->symbol.value -= sym->symbol.section->vma;

	  switch (ELF_ST_BIND (isym->st_info))
	    {
	    case STB_LOCAL:
	      sym->symbol.flags |= BSF_LOCAL;
	      break;
	    case STB_GLOBAL:
	      if (isym->st_shndx != SHN_UNDEF && isym->st_shndx != SHN_COMMON)
		sym->symbol.flags |= BSF_GLOBAL;
	      break;
	    case STB_WEAK:
	      sym->symbol.flags |= BSF_WEAK;
	      break;
	    case STB_GNU_UNIQUE:
	      sym->symbol.flags |= BSF_GNU_UNIQUE;
	      break;
	    }

	  switch (ELF_ST_TYPE (isym->st_info))
	    {
	    case STT_SECTION:
	      /* Mark the input section symbol as used since it may be
	         used for relocation and section group.
		 NB: BSF_SECTION_SYM_USED is ignored by linker and may
		 be cleared by objcopy for non-relocatable inputs.  */
	      sym->symbol.flags |= (BSF_SECTION_SYM
				    | BSF_DEBUGGING
				    | BSF_SECTION_SYM_USED);
	      break;
	    case STT_FILE:
	      sym->symbol.flags |= BSF_FILE | BSF_DEBUGGING;
	      break;
	    case STT_FUNC:
	      sym->symbol.flags |= BSF_FUNCTION;
	      break;
	    case STT_COMMON:
	      /* FIXME: Do we have to put the size field into the value field
		 as we do with symbols in SHN_COMMON sections (see above) ?  */
	      sym->symbol.flags |= BSF_ELF_COMMON;
	      /* Fall through.  */
	    case STT_OBJECT:
	      sym->symbol.flags |= BSF_OBJECT;
	      break;
	    case STT_TLS:
	      sym->symbol.flags |= BSF_THREAD_LOCAL;
	      break;
	    case STT_RELC:
	      sym->symbol.flags |= BSF_RELC;
	      break;
	    case STT_SRELC:
	      sym->symbol.flags |= BSF_SRELC;
	      break;
	    case STT_GNU_IFUNC:
	      sym->symbol.flags |= BSF_GNU_INDIRECT_FUNCTION;
	      break;
	    }

	  if (dynamic)
	    sym->symbol.flags |= BSF_DYNAMIC;

	  if (elf_tdata (abfd)->dt_versym)
	    sym->version = bfd_get_16 (abfd,
				       elf_tdata (abfd)->dt_versym + 2 * i);
	  else if (xver != NULL)
	    {
	      Elf_Internal_Versym iversym;

	      _bfd_elf_swap_versym_in (abfd, xver, &iversym);
	      sym->version = iversym.vs_vers;
	      xver++;
	    }

	  /* Do some backend-specific processing on this symbol.  */
	  if (ebd->elf_backend_symbol_processing)
	    (*ebd->elf_backend_symbol_processing) (abfd, &sym->symbol);
	}
    }

  /* Do some backend-specific processing on this symbol table.  */
  if (ebd->elf_backend_symbol_table_processing)
    (*ebd->elf_backend_symbol_table_processing) (abfd, symbase, symcount);

  /* We rely on the zalloc to clear out the final symbol entry.  */

  symcount = sym - symbase;

  /* Fill in the user's symbol pointer vector if needed.  */
  if (symptrs)
    {
      long l = symcount;

      sym = symbase;
      while (l-- > 0)
	{
	  *symptrs++ = &sym->symbol;
	  sym++;
	}
      *symptrs = 0;		/* Final null pointer */
    }

  free (xverbuf);
  if (hdr->contents != (unsigned char *) isymbuf
      && !elf_use_dt_symtab_p (abfd))
    free (isymbuf);
  return symcount;

 error_return:
  free (xverbuf);
  if (hdr->contents != (unsigned char *) isymbuf
      && !elf_use_dt_symtab_p (abfd))
    free (isymbuf);
  return -1;
}

/* Read relocations for ASECT from REL_HDR.  There are RELOC_COUNT of
   them.  */

static bool
elf_slurp_reloc_table_from_section (bfd *abfd,
				    asection *asect,
				    Elf_Internal_Shdr *rel_hdr,
				    bfd_size_type reloc_count,
				    arelent *relents,
				    asymbol **symbols,
				    bool dynamic)
{
  const struct elf_backend_data * const ebd = get_elf_backend_data (abfd);
  void *allocated = NULL;
  bfd_byte *native_relocs;
  arelent *relent;
  unsigned int i;
  int entsize;
  unsigned int symcount;

  if (bfd_seek (abfd, rel_hdr->sh_offset, SEEK_SET) != 0)
    return false;
  allocated = _bfd_malloc_and_read (abfd, rel_hdr->sh_size, rel_hdr->sh_size);
  if (allocated == NULL)
    return false;

  native_relocs = (bfd_byte *) allocated;

  entsize = rel_hdr->sh_entsize;
  BFD_ASSERT (entsize == sizeof (Elf_External_Rel)
	      || entsize == sizeof (Elf_External_Rela));

  if (dynamic)
    symcount = bfd_get_dynamic_symcount (abfd);
  else
    symcount = bfd_get_symcount (abfd);

  for (i = 0, relent = relents;
       i < reloc_count;
       i++, relent++, native_relocs += entsize)
    {
      bool res;
      Elf_Internal_Rela rela;

      if (entsize == sizeof (Elf_External_Rela))
	elf_swap_reloca_in (abfd, native_relocs, &rela);
      else
	elf_swap_reloc_in (abfd, native_relocs, &rela);

      /* The address of an ELF reloc is section relative for an object
	 file, and absolute for an executable file or shared library.
	 The address of a normal BFD reloc is always section relative,
	 and the address of a dynamic reloc is absolute..  */
      if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0 || dynamic)
	relent->address = rela.r_offset;
      else
	relent->address = rela.r_offset - asect->vma;

      if (ELF_R_SYM (rela.r_info) == STN_UNDEF)
	/* FIXME: This and the error case below mean that we have a
	   symbol on relocs that is not elf_symbol_type.  */
	relent->sym_ptr_ptr = &bfd_abs_section_ptr->symbol;
      else if (ELF_R_SYM (rela.r_info) > symcount)
	{
	  _bfd_error_handler
	    /* xgettext:c-format */
	    (_("%pB(%pA): relocation %d has invalid symbol index %ld"),
	     abfd, asect, i, (long) ELF_R_SYM (rela.r_info));
	  bfd_set_error (bfd_error_bad_value);
	  relent->sym_ptr_ptr = &bfd_abs_section_ptr->symbol;
	}
      else
	{
	  asymbol **ps;

	  ps = symbols + ELF_R_SYM (rela.r_info) - 1;

	  relent->sym_ptr_ptr = ps;
	}

      relent->addend = rela.r_addend;

      res = false;
      if ((entsize == sizeof (Elf_External_Rela)
	   || ebd->elf_info_to_howto_rel == NULL)
	  && ebd->elf_info_to_howto != NULL)
	res = ebd->elf_info_to_howto (abfd, relent, &rela);
      else if (ebd->elf_info_to_howto_rel != NULL)
	res = ebd->elf_info_to_howto_rel (abfd, relent, &rela);

      if (! res || relent->howto == NULL)
	goto error_return;
    }

  free (allocated);
  return true;

 error_return:
  free (allocated);
  return false;
}

/* Read in and swap the external relocs.  */

bool
elf_slurp_reloc_table (bfd *abfd,
		       asection *asect,
		       asymbol **symbols,
		       bool dynamic)
{
  const struct elf_backend_data * const bed = get_elf_backend_data (abfd);
  struct bfd_elf_section_data * const d = elf_section_data (asect);
  Elf_Internal_Shdr *rel_hdr;
  Elf_Internal_Shdr *rel_hdr2;
  bfd_size_type reloc_count;
  bfd_size_type reloc_count2;
  arelent *relents;
  size_t amt;

  if (asect->relocation != NULL)
    return true;

  if (! dynamic)
    {
      if ((asect->flags & SEC_RELOC) == 0
	  || asect->reloc_count == 0)
	return true;

      rel_hdr = d->rel.hdr;
      reloc_count = rel_hdr ? NUM_SHDR_ENTRIES (rel_hdr) : 0;
      rel_hdr2 = d->rela.hdr;
      reloc_count2 = rel_hdr2 ? NUM_SHDR_ENTRIES (rel_hdr2) : 0;

      /* PR 17512: file: 0b4f81b7.  */
      if (asect->reloc_count != reloc_count + reloc_count2)
	return false;
      BFD_ASSERT ((rel_hdr && asect->rel_filepos == rel_hdr->sh_offset)
		  || (rel_hdr2 && asect->rel_filepos == rel_hdr2->sh_offset));

    }
  else
    {
      /* Note that ASECT->RELOC_COUNT tends not to be accurate in this
	 case because relocations against this section may use the
	 dynamic symbol table, and in that case bfd_section_from_shdr
	 in elf.c does not update the RELOC_COUNT.  */
      if (asect->size == 0)
	return true;

      rel_hdr = &d->this_hdr;
      reloc_count = NUM_SHDR_ENTRIES (rel_hdr);
      rel_hdr2 = NULL;
      reloc_count2 = 0;
    }

  if (_bfd_mul_overflow (reloc_count + reloc_count2, sizeof (arelent), &amt))
    {
      bfd_set_error (bfd_error_file_too_big);
      return false;
    }
  relents = (arelent *) bfd_alloc (abfd, amt);
  if (relents == NULL)
    return false;

  if (rel_hdr
      && !elf_slurp_reloc_table_from_section (abfd, asect,
					      rel_hdr, reloc_count,
					      relents,
					      symbols, dynamic))
    return false;

  if (rel_hdr2
      && !elf_slurp_reloc_table_from_section (abfd, asect,
					      rel_hdr2, reloc_count2,
					      relents + reloc_count,
					      symbols, dynamic))
    return false;

  if (!bed->slurp_secondary_relocs (abfd, asect, symbols, dynamic))
    return false;

  asect->relocation = relents;
  return true;
}

#if DEBUG & 2
static void
elf_debug_section (int num, Elf_Internal_Shdr *hdr)
{
  fprintf (stderr, "\nSection#%d '%s' 0x%.8lx\n", num,
	   hdr->bfd_section != NULL ? hdr->bfd_section->name : "",
	   (long) hdr);
  fprintf (stderr,
	   "sh_name      = %ld\tsh_type      = %ld\tsh_flags     = %ld\n",
	   (long) hdr->sh_name,
	   (long) hdr->sh_type,
	   (long) hdr->sh_flags);
  fprintf (stderr,
	   "sh_addr      = %ld\tsh_offset    = %ld\tsh_size      = %ld\n",
	   (long) hdr->sh_addr,
	   (long) hdr->sh_offset,
	   (long) hdr->sh_size);
  fprintf (stderr,
	   "sh_link      = %ld\tsh_info      = %ld\tsh_addralign = %ld\n",
	   (long) hdr->sh_link,
	   (long) hdr->sh_info,
	   (long) hdr->sh_addralign);
  fprintf (stderr, "sh_entsize   = %ld\n",
	   (long) hdr->sh_entsize);
  fflush (stderr);
}
#endif

#if DEBUG & 1
static void
elf_debug_file (Elf_Internal_Ehdr *ehdrp)
{
  fprintf (stderr, "e_entry      = 0x%.8lx\n", (long) ehdrp->e_entry);
  fprintf (stderr, "e_phoff      = %ld\n", (long) ehdrp->e_phoff);
  fprintf (stderr, "e_phnum      = %ld\n", (long) ehdrp->e_phnum);
  fprintf (stderr, "e_phentsize  = %ld\n", (long) ehdrp->e_phentsize);
  fprintf (stderr, "e_shoff      = %ld\n", (long) ehdrp->e_shoff);
  fprintf (stderr, "e_shnum      = %ld\n", (long) ehdrp->e_shnum);
  fprintf (stderr, "e_shentsize  = %ld\n", (long) ehdrp->e_shentsize);
}
#endif

/* Create a new BFD as if by bfd_openr.  Rather than opening a file,
   reconstruct an ELF file by reading the segments out of remote
   memory based on the ELF file header at EHDR_VMA and the ELF program
   headers it points to.  If non-zero, SIZE is the known extent of the
   object.  If not null, *LOADBASEP is filled in with the difference
   between the VMAs from which the segments were read, and the VMAs
   the file headers (and hence BFD's idea of each section's VMA) put
   them at.

   The function TARGET_READ_MEMORY is called to copy LEN bytes from
   the remote memory at target address VMA into the local buffer at
   MYADDR; it should return zero on success or an `errno' code on
   failure.  TEMPL must be a BFD for a target with the word size and
   byte order found in the remote memory.  */

bfd *
NAME(_bfd_elf,bfd_from_remote_memory)
  (bfd *templ,
   bfd_vma ehdr_vma    /* Bytes.  */,
   bfd_size_type size  /* Octets.  */,
   bfd_vma *loadbasep  /* Bytes.  */,
   int (*target_read_memory) (bfd_vma, bfd_byte *, bfd_size_type))
                          /* (Bytes  ,           , octets       ).  */
{
  Elf_External_Ehdr x_ehdr;	/* Elf file header, external form */
  Elf_Internal_Ehdr i_ehdr;	/* Elf file header, internal form */
  Elf_External_Phdr *x_phdrs;
  Elf_Internal_Phdr *i_phdrs, *last_phdr, *first_phdr;
  bfd *nbfd;
  struct bfd_in_memory *bim;
  bfd_byte *contents;
  int err;
  unsigned int i;
  bfd_vma high_offset;
  bfd_vma shdr_end;
  bfd_vma loadbase;  /* Bytes.  */
  size_t amt;
  unsigned int opb = bfd_octets_per_byte (templ, NULL);

  /* Read in the ELF header in external format.  */
  err = target_read_memory (ehdr_vma, (bfd_byte *) &x_ehdr, sizeof x_ehdr);
  if (err)
    {
      bfd_set_error (bfd_error_system_call);
      errno = err;
      return NULL;
    }

  /* Now check to see if we have a valid ELF file, and one that BFD can
     make use of.  The magic number must match, the address size ('class')
     and byte-swapping must match our XVEC entry.  */

  if (! elf_file_p (&x_ehdr)
      || x_ehdr.e_ident[EI_VERSION] != EV_CURRENT
      || x_ehdr.e_ident[EI_CLASS] != ELFCLASS)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* Check that file's byte order matches xvec's */
  switch (x_ehdr.e_ident[EI_DATA])
    {
    case ELFDATA2MSB:		/* Big-endian */
      if (! bfd_header_big_endian (templ))
	{
	  bfd_set_error (bfd_error_wrong_format);
	  return NULL;
	}
      break;
    case ELFDATA2LSB:		/* Little-endian */
      if (! bfd_header_little_endian (templ))
	{
	  bfd_set_error (bfd_error_wrong_format);
	  return NULL;
	}
      break;
    case ELFDATANONE:		/* No data encoding specified */
    default:			/* Unknown data encoding specified */
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  elf_swap_ehdr_in (templ, &x_ehdr, &i_ehdr);

  /* The file header tells where to find the program headers.
     These are what we use to actually choose what to read.  */

  if (i_ehdr.e_phentsize != sizeof (Elf_External_Phdr) || i_ehdr.e_phnum == 0)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (_bfd_mul_overflow (i_ehdr.e_phnum,
			 sizeof (*x_phdrs) + sizeof (*i_phdrs), &amt))
    {
      bfd_set_error (bfd_error_file_too_big);
      return NULL;
    }
  x_phdrs = (Elf_External_Phdr *) bfd_malloc (amt);
  if (x_phdrs == NULL)
    return NULL;
  err = target_read_memory (ehdr_vma + i_ehdr.e_phoff, (bfd_byte *) x_phdrs,
			    i_ehdr.e_phnum * sizeof x_phdrs[0]);
  if (err)
    {
      free (x_phdrs);
      bfd_set_error (bfd_error_system_call);
      errno = err;
      return NULL;
    }
  i_phdrs = (Elf_Internal_Phdr *) &x_phdrs[i_ehdr.e_phnum];

  high_offset = 0;
  loadbase = 0;
  first_phdr = NULL;
  last_phdr = NULL;
  for (i = 0; i < i_ehdr.e_phnum; ++i)
    {
      elf_swap_phdr_in (templ, &x_phdrs[i], &i_phdrs[i]);
      if (i_phdrs[i].p_type == PT_LOAD)
	{
	  bfd_vma segment_end = i_phdrs[i].p_offset + i_phdrs[i].p_filesz;

	  if (segment_end > high_offset)
	    {
	      high_offset = segment_end;
	      last_phdr = &i_phdrs[i];
	    }

	  /* If this program header covers offset zero, where the file
	     header sits, then we can figure out the loadbase.  */
	  if (first_phdr == NULL)
	    {
	      bfd_vma p_offset = i_phdrs[i].p_offset;  /* Octets.  */
	      bfd_vma p_vaddr = i_phdrs[i].p_vaddr;    /* Octets.  */

	      if (i_phdrs[i].p_align > 1)
		{
		  p_offset &= -(i_phdrs[i].p_align * opb);
		  p_vaddr &= -(i_phdrs[i].p_align * opb);
		}
	      if (p_offset == 0)
		{
		  loadbase = ehdr_vma - p_vaddr / opb;
		  first_phdr = &i_phdrs[i];
		}
	    }
	}
    }
  if (high_offset == 0)
    {
      /* There were no PT_LOAD segments, so we don't have anything to read.  */
      free (x_phdrs);
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  shdr_end = 0;
  if (i_ehdr.e_shoff != 0 && i_ehdr.e_shnum != 0 && i_ehdr.e_shentsize != 0)
    {
      shdr_end = i_ehdr.e_shoff + i_ehdr.e_shnum * i_ehdr.e_shentsize;

      if (last_phdr->p_filesz != last_phdr->p_memsz)
	{
	  /* If the last PT_LOAD header has a bss area then ld.so will
	     have cleared anything past p_filesz, zapping the section
	     headers.  */
	}
      else if (size >= shdr_end)
	high_offset = size;
      else
	{
	  bfd_vma page_size = get_elf_backend_data (templ)->minpagesize;
	  bfd_vma segment_end = last_phdr->p_offset + last_phdr->p_filesz;

	  /* Assume we loaded full pages, allowing us to sometimes see
	     section headers.  */
	  if (page_size > 1 && shdr_end > segment_end)
	    {
	      bfd_vma page_end = (segment_end + page_size - 1) & -page_size;

	      if (page_end >= shdr_end)
		/* Whee, section headers covered.  */
		high_offset = shdr_end;
	    }
	}
    }

  /* Now we know the size of the whole image we want read in.  */
  contents = (bfd_byte *) bfd_zmalloc (high_offset);
  if (contents == NULL)
    {
      free (x_phdrs);
      return NULL;
    }

  for (i = 0; i < i_ehdr.e_phnum; ++i)
    if (i_phdrs[i].p_type == PT_LOAD)
      {
	bfd_vma start = i_phdrs[i].p_offset;         /* Octets.  */
	bfd_vma end = start + i_phdrs[i].p_filesz;   /* Octets.  */
	bfd_vma vaddr = i_phdrs[i].p_vaddr;          /* Octets.  */

	/* Extend the beginning of the first pt_load to cover file
	   header and program headers, if we proved earlier that its
	   aligned offset is 0.  */
	if (first_phdr == &i_phdrs[i])
	  {
	    vaddr -= start;
	    start = 0;
	  }
	/* Extend the end of the last pt_load to cover section headers.  */
	if (last_phdr == &i_phdrs[i])
	  end = high_offset;
	err = target_read_memory (loadbase + vaddr / opb,
				  contents + start, end - start);
	if (err)
	  {
	    free (x_phdrs);
	    free (contents);
	    bfd_set_error (bfd_error_system_call);
	    errno = err;
	    return NULL;
	  }
      }
  free (x_phdrs);

  /* If the segments visible in memory didn't include the section headers,
     then clear them from the file header.  */
  if (high_offset < shdr_end)
    {
      memset (&x_ehdr.e_shoff, 0, sizeof x_ehdr.e_shoff);
      memset (&x_ehdr.e_shnum, 0, sizeof x_ehdr.e_shnum);
      memset (&x_ehdr.e_shstrndx, 0, sizeof x_ehdr.e_shstrndx);
    }

  /* This will normally have been in the first PT_LOAD segment.  But it
     conceivably could be missing, and we might have just changed it.  */
  memcpy (contents, &x_ehdr, sizeof x_ehdr);

  /* Now we have a memory image of the ELF file contents.  Make a BFD.  */
  bim = (struct bfd_in_memory *) bfd_malloc (sizeof (struct bfd_in_memory));
  if (bim == NULL)
    {
      free (contents);
      return NULL;
    }
  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL
      || !bfd_set_filename (nbfd, "<in-memory>"))
    {
      free (bim);
      free (contents);
      return NULL;
    }
  nbfd->xvec = templ->xvec;
  bim->size = high_offset;
  bim->buffer = contents;
  nbfd->iostream = bim;
  nbfd->flags = BFD_IN_MEMORY;
  nbfd->iovec = &_bfd_memory_iovec;
  nbfd->origin = 0;
  nbfd->direction = read_direction;
  nbfd->mtime = time (NULL);
  nbfd->mtime_set = true;

  if (loadbasep)
    *loadbasep = loadbase;
  return nbfd;
}

/* Function for ELF_R_INFO.  */

bfd_vma
NAME(elf,r_info) (bfd_vma sym, bfd_vma type)
{
  return ELF_R_INFO (sym, type);
}

/* Function for ELF_R_SYM.  */

bfd_vma
NAME(elf,r_sym) (bfd_vma r_info)
{
  return ELF_R_SYM (r_info);
}

#include "elfcore.h"

/* Size-dependent data and functions.  */
const struct elf_size_info NAME(_bfd_elf,size_info) = {
  sizeof (Elf_External_Ehdr),
  sizeof (Elf_External_Phdr),
  sizeof (Elf_External_Shdr),
  sizeof (Elf_External_Rel),
  sizeof (Elf_External_Rela),
  sizeof (Elf_External_Sym),
  sizeof (Elf_External_Dyn),
  sizeof (Elf_External_Note),
  4,
  1,
  ARCH_SIZE, LOG_FILE_ALIGN,
  ELFCLASS, EV_CURRENT,
  elf_write_out_phdrs,
  elf_write_shdrs_and_ehdr,
  elf_checksum_contents,
  elf_write_relocs,
  elf_swap_symbol_in,
  elf_swap_symbol_out,
  elf_slurp_reloc_table,
  elf_slurp_symbol_table,
  elf_swap_dyn_in,
  elf_swap_dyn_out,
  elf_swap_reloc_in,
  elf_swap_reloc_out,
  elf_swap_reloca_in,
  elf_swap_reloca_out
};
