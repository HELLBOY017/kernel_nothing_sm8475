/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Module internals
 *
 * Copyright (C) 2012 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */

#include <linux/elf.h>
#include <asm/module.h>

struct load_info {
	const char *name;
	/* pointer to module in temporary copy, freed at end of load_module() */
	struct module *mod;
	Elf_Ehdr *hdr;
	unsigned long len;
	Elf_Shdr *sechdrs;
	char *secstrings, *strtab;
	unsigned long symoffs, stroffs, init_typeoffs, core_typeoffs;
	struct _ddebug *debug;
	unsigned int num_debug;
	bool sig_ok;
#ifdef CONFIG_KALLSYMS
	unsigned long mod_kallsyms_init_off;
#endif
	struct {
		unsigned int sym, str, mod, vers, info, pcpu;
	} index;
};

extern int mod_verify_sig(const void *mod, struct load_info *info);

/* Find a module section: 0 means not found. */
static unsigned int find_sec(const struct load_info *info, const char *name)
{
	unsigned int i;

	for (i = 1; i < info->hdr->e_shnum; i++) {
		Elf_Shdr *shdr = &info->sechdrs[i];
		/* Alloc bit cleared means "ignore it." */
		if ((shdr->sh_flags & SHF_ALLOC)
		    && strcmp(info->secstrings + shdr->sh_name, name) == 0)
			return i;
	}
	return 0;
}

static unsigned int find_pcpusec(struct load_info *info)
{
	return find_sec(info, ".data..percpu");
}

/* Parse tag=value strings from .modinfo section */
static char *next_string(char *string, unsigned long *secsize)
{
	/* Skip non-zero chars */
	while (string[0]) {
		string++;
		if ((*secsize)-- <= 1)
			return NULL;
	}

	/* Skip any zero padding. */
	while (!string[0]) {
		string++;
		if ((*secsize)-- <= 1)
			return NULL;
	}
	return string;
}

static char *get_next_modinfo(const struct load_info *info, const char *tag,
			      char *prev)
{
	char *p;
	unsigned int taglen = strlen(tag);
	Elf_Shdr *infosec = &info->sechdrs[info->index.info];
	unsigned long size = infosec->sh_size;

	/*
	 * get_modinfo() calls made before rewrite_section_headers()
	 * must use sh_offset, as sh_addr isn't set!
	 */
	char *modinfo = (char *)info->hdr + infosec->sh_offset;

	if (prev) {
		size -= prev - modinfo;
		modinfo = next_string(prev, &size);
	}

	for (p = modinfo; p; p = next_string(p, &size)) {
		if (strncmp(p, tag, taglen) == 0 && p[taglen] == '=')
			return p + taglen + 1;
	}
	return NULL;
}

static char *get_modinfo(const struct load_info *info, const char *tag)
{
	return get_next_modinfo(info, tag, NULL);
}

static int validate_section_offset(struct load_info *info, Elf_Shdr *shdr)
{
	unsigned long secend;

	/*
	 * Check for both overflow and offset/size being
	 * too large.
	 */
	secend = shdr->sh_offset + shdr->sh_size;
	if (secend < shdr->sh_offset || secend > info->len)
		return -ENOEXEC;

	return 0;
}

/*
 * Sanity checks against invalid binaries, wrong arch, weird elf version.
 *
 * Also do basic validity checks against section offsets and sizes, the
 * section name string table, and the indices used for it (sh_name).
 */
static int elf_validity_check(struct load_info *info)
{
	unsigned int i;
	Elf_Shdr *shdr, *strhdr;
	int err;

	if (info->len < sizeof(*(info->hdr)))
		return -ENOEXEC;

	if (memcmp(info->hdr->e_ident, ELFMAG, SELFMAG) != 0
	    || info->hdr->e_type != ET_REL
	    || !elf_check_arch(info->hdr)
	    || info->hdr->e_shentsize != sizeof(Elf_Shdr))
		return -ENOEXEC;

	/*
	 * e_shnum is 16 bits, and sizeof(Elf_Shdr) is
	 * known and small. So e_shnum * sizeof(Elf_Shdr)
	 * will not overflow unsigned long on any platform.
	 */
	if (info->hdr->e_shoff >= info->len
	    || (info->hdr->e_shnum * sizeof(Elf_Shdr) >
		info->len - info->hdr->e_shoff))
		return -ENOEXEC;

	info->sechdrs = (void *)info->hdr + info->hdr->e_shoff;

	/*
	 * Verify if the section name table index is valid.
	 */
	if (info->hdr->e_shstrndx == SHN_UNDEF
	    || info->hdr->e_shstrndx >= info->hdr->e_shnum)
		return -ENOEXEC;

	strhdr = &info->sechdrs[info->hdr->e_shstrndx];
	err = validate_section_offset(info, strhdr);
	if (err < 0)
		return err;

	/*
	 * The section name table must be NUL-terminated, as required
	 * by the spec. This makes strcmp and pr_* calls that access
	 * strings in the section safe.
	 */
	info->secstrings = (void *)info->hdr + strhdr->sh_offset;
	if (info->secstrings[strhdr->sh_size - 1] != '\0')
		return -ENOEXEC;

	/*
	 * The code assumes that section 0 has a length of zero and
	 * an addr of zero, so check for it.
	 */
	if (info->sechdrs[0].sh_type != SHT_NULL
	    || info->sechdrs[0].sh_size != 0
	    || info->sechdrs[0].sh_addr != 0)
		return -ENOEXEC;

	for (i = 1; i < info->hdr->e_shnum; i++) {
		shdr = &info->sechdrs[i];
		switch (shdr->sh_type) {
		case SHT_NULL:
		case SHT_NOBITS:
			continue;
		case SHT_SYMTAB:
			if (shdr->sh_link == SHN_UNDEF
			    || shdr->sh_link >= info->hdr->e_shnum)
				return -ENOEXEC;
			fallthrough;
		default:
			err = validate_section_offset(info, shdr);
			if (err < 0) {
				pr_err("Invalid ELF section in module (section %u type %u)\n",
					i, shdr->sh_type);
				return err;
			}

			if (shdr->sh_flags & SHF_ALLOC) {
				if (shdr->sh_name >= strhdr->sh_size) {
					pr_err("Invalid ELF section name in module (section %u type %u)\n",
					       i, shdr->sh_type);
					return -ENOEXEC;
				}
			}
			break;
		}
	}

	return 0;
}

#define COPY_CHUNK_SIZE (16*PAGE_SIZE)

static int copy_chunked_from_user(void *dst, const void __user *usrc, unsigned long len)
{
	do {
		unsigned long n = min(len, COPY_CHUNK_SIZE);

		if (copy_from_user(dst, usrc, n) != 0)
			return -EFAULT;
		cond_resched();
		dst += n;
		usrc += n;
		len -= n;
	} while (len);
	return 0;
}

/* Sets info->hdr and info->len. */
static int copy_module_from_user(const void __user *umod, unsigned long len,
				  struct load_info *info)
{
	int err;

	info->len = len;
	if (info->len < sizeof(*(info->hdr)))
		return -ENOEXEC;

	err = security_kernel_load_data(LOADING_MODULE, true);
	if (err)
		return err;

	/* Suck in entire file: we'll want most of it. */
	info->hdr = __vmalloc(info->len, GFP_KERNEL | __GFP_NOWARN);
	if (!info->hdr)
		return -ENOMEM;

	if (copy_chunked_from_user(info->hdr, umod, info->len) != 0) {
		err = -EFAULT;
		goto out;
	}

	err = security_kernel_post_load_data((char *)info->hdr, info->len,
					     LOADING_MODULE, "init_module");
out:
	if (err)
		vfree(info->hdr);

	return err;
}

static void free_copy(struct load_info *info)
{
	vfree(info->hdr);
}

/*
 * Set up our basic convenience variables (pointers to section headers,
 * search for module section index etc), and do some basic section
 * verification.
 *
 * Set info->mod to the temporary copy of the module in info->hdr. The final one
 * will be allocated in move_module().
 */
static int setup_load_info(struct load_info *info, int flags)
{
	unsigned int i;

	/* Try to find a name early so we can log errors with a module name */
	info->index.info = find_sec(info, ".modinfo");
	if (info->index.info)
		info->name = get_modinfo(info, "name");

	/* Find internal symbols and strings. */
	for (i = 1; i < info->hdr->e_shnum; i++) {
		if (info->sechdrs[i].sh_type == SHT_SYMTAB) {
			info->index.sym = i;
			info->index.str = info->sechdrs[i].sh_link;
			info->strtab = (char *)info->hdr
				+ info->sechdrs[info->index.str].sh_offset;
			break;
		}
	}

	if (info->index.sym == 0) {
		pr_warn("%s: module has no symbols (stripped?)\n",
			info->name ?: "(missing .modinfo section or name field)");
		return -ENOEXEC;
	}

	info->index.mod = find_sec(info, ".gnu.linkonce.this_module");
	if (!info->index.mod) {
		pr_warn("%s: No module found in object\n",
			info->name ?: "(missing .modinfo section or name field)");
		return -ENOEXEC;
	}
	/* This is temporary: point mod into copy of data. */
	info->mod = (void *)info->hdr + info->sechdrs[info->index.mod].sh_offset;

	/*
	 * If we didn't load the .modinfo 'name' field earlier, fall back to
	 * on-disk struct mod 'name' field.
	 */
	if (!info->name)
		info->name = info->mod->name;

	if (flags & MODULE_INIT_IGNORE_MODVERSIONS)
		info->index.vers = 0; /* Pretend no __versions section! */
	else
		info->index.vers = find_sec(info, "__versions");

	info->index.pcpu = find_pcpusec(info);

	return 0;
}
