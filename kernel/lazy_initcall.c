// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Juhyung Park <qkrwngud825@gmail.com>
 *
 * Partially based on kernel/module.c.
 */

#ifdef CONFIG_LAZY_INITCALL_DEBUG
#define DEBUG
#define __fatal pr_err
#else
#define __fatal panic
#endif

#define pr_fmt(fmt) "lazy_initcall: " fmt

#include <linux/syscalls.h>
#include <linux/kmemleak.h>
#include <uapi/linux/module.h>

#include "module-internal.h"

#ifdef CONFIG_LAZY_INITCALL_DEBUG
static void __init show_unused_modules(struct work_struct *unused);
static __initdata DECLARE_DELAYED_WORK(show_unused_work, show_unused_modules);
#endif
static DEFINE_MUTEX(lazy_initcall_mutex);
static bool completed;

/*
 * Why is this here, instead of defconfig?
 *
 * Data used in defconfig isn't freed in free_initmem() and putting a list this
 * big into the defconfig isn't really ideal anyways.
 *
 * Since lazy_initcall isn't meant to be generic, set this here.
 *
 * This list is generatable by putting .ko modules from vendor, vendor_boot and
 * vendor_dlkm to a directory and running the following:
 *
 * MODDIR=/path/to/modules
 * find "$MODDIR" -name '*.ko' -exec modinfo {} + | grep '^name:' | awk '{print $2}' | sort | uniq | while read f; do printf '\t"'$f'",\n'; done
 * find "$MODDIR" -name '*.ko' | while read f; do if ! modinfo $f | grep -q "^name:"; then n=$(basename $f); n="${n%.*}"; printf '\t"'$n'",\n'; fi; done | sort | uniq
 */

static const __initconst char * const targets_list[] = {
	NULL
};

/*
 * Some drivers don't have module_init(), which will lead to lookup failure
 * from lazy_initcall when a module with the same name is asked to be loaded
 * from the userspace.
 *
 * Lazy initcall can optionally maintain a list of kernel drivers built into
 * the kernel that matches the name from the firmware so that those aren't
 * treated as errors.
 *
 * Ew, is this the best approach?
 *
 * Detecting the presense of .init.text section or initcall_t function is
 * unreliable as .init.text might not exist when a driver doesn't use __init
 * and modpost adds an empty .init stub even if a driver doesn't declare a
 * function for module_init().
 *
 * This list is generatable by putting .ko modules from vendor, vendor_boot and
 * vendor_dlkm to a directory, cd'ing to kernel's O directory and running the
 * following:
 *
 * MODDIR=/path/to/modules
 * find -name '*.o' | tr '-' '_' > list
 * find "$MODDIR" -name '*.ko' -exec modinfo {} + | grep '^name:' | awk '{print $2}' | sort | uniq | while read f; do if grep -q /"$f".o list; then printf '\t"'$f'",\n'; fi; done
 */
static const __initconst char * const builtin_list[] = {
	NULL,
};

/*
 * Some drivers behave differently when it's built-in, so deferring its
 * initialization causes issues.
 *
 * Put those to this blacklist so that it is ignored from lazy_initcall.
 *
 * You can also use this as an ignorelist.
 */
static const __initconst char * const blacklist[] = {
	NULL
};

/*
 * You may want some specific drivers to load after all lazy modules have been
 * loaded.
 *
 * Add them here.
 */
static const __initconst char * const deferred_list[] = {
	NULL
};

static struct lazy_initcall __initdata lazy_initcalls[ARRAY_SIZE(targets_list) - ARRAY_SIZE(blacklist) + ARRAY_SIZE(deferred_list)];
static int __initdata counter;

bool __init add_lazy_initcall(initcall_t fn, char modname[], char filename[])
{
	int i;
	bool match = false;
	enum lazy_initcall_type type = NORMAL;

	for (i = 0; blacklist[i]; i++) {
		if (!strcmp(blacklist[i], modname))
			return false;
	}

	for (i = 0; targets_list[i]; i++) {
		if (!strcmp(targets_list[i], modname)) {
			match = true;
			break;
		}
	}

	for (i = 0; deferred_list[i]; i++) {
		if (!strcmp(deferred_list[i], modname)) {
			match = true;
			type = DEFERRED;
			break;
		}
	}

	if (!match)
		return false;

	mutex_lock(&lazy_initcall_mutex);

	pr_debug("adding lazy_initcalls[%d] from %s - %s\n",
				counter, modname, filename);

	lazy_initcalls[counter].fn = fn;
	lazy_initcalls[counter].modname = modname;
	lazy_initcalls[counter].filename = filename;
	lazy_initcalls[counter].type = type;
	counter++;

	mutex_unlock(&lazy_initcall_mutex);

	return true;
}

#ifdef CONFIG_LAZY_INITCALL_DEBUG
static void __init show_unused_modules(struct work_struct *unused)
{
	int i;

	for (i = 0; i < counter; i++) {
		if (!lazy_initcalls[i].loaded) {
			pr_info("lazy_initcalls[%d]: %s not loaded yet\n", i, lazy_initcalls[i].modname);
		}
	}

	queue_delayed_work(system_freezable_power_efficient_wq,
			&show_unused_work, 5 * HZ);
}
#endif

static int __init unknown_integrated_module_param_cb(char *param, char *val,
					      const char *modname, void *arg)
{
	pr_err("%s: unknown parameter '%s' ignored\n", modname, param);
	return 0;
}

static int __init integrated_module_param_cb(char *param, char *val,
				      const char *modname, void *arg)
{
	size_t nlen, plen, vlen;
	char *modparam;

	nlen = strlen(modname);
	plen = strlen(param);
	vlen = val ? strlen(val) : 0;
	if (vlen)
		/* Parameter formatted as "modname.param=val" */
		modparam = kmalloc(nlen + plen + vlen + 3, GFP_KERNEL);
	else
		/* Parameter formatted as "modname.param" */
		modparam = kmalloc(nlen + plen + 2, GFP_KERNEL);
	if (!modparam) {
		pr_err("%s: allocation failed for module '%s' parameter '%s'\n",
		       __func__, modname, param);
		return 0;
	}

	/* Construct the correct parameter name for the built-in module */
	memcpy(modparam, modname, nlen);
	modparam[nlen] = '.';
	memcpy(&modparam[nlen + 1], param, plen);
	if (vlen) {
		modparam[nlen + 1 + plen] = '=';
		memcpy(&modparam[nlen + 1 + plen + 1], val, vlen);
		modparam[nlen + 1 + plen + 1 + vlen] = '\0';
	} else {
		modparam[nlen + 1 + plen] = '\0';
	}

	/* Now have parse_args() look for the correct parameter name */
	parse_args(modname, modparam, __start___param,
		   __stop___param - __start___param,
		   -32768, 32767, NULL,
		   unknown_integrated_module_param_cb);
	kfree(modparam);
	return 0;
}

static noinline void __init load_modname(const char * const modname, const char __user *uargs)
{
	int i, ret;
	bool match = false;
	char *args;
	initcall_t fn;

	pr_debug("trying to load \"%s\"\n", modname);

	// Check if the driver is blacklisted (built-in, but not lazy-loaded)
	for (i = 0; blacklist[i]; i++) {
		if (!strcmp(blacklist[i], modname)) {
			pr_debug("\"%s\" is blacklisted (not lazy-loaded)\n", modname);
			return;
		}
	}

	// Find the function pointer from lazy_initcalls[]
	for (i = 0; i < counter; i++) {
		if (!strcmp(lazy_initcalls[i].modname, modname)) {
			fn = lazy_initcalls[i].fn;
			if (lazy_initcalls[i].loaded) {
				pr_debug("lazy_initcalls[%d]: %s already loaded\n", i, modname);
				return;
			}
			lazy_initcalls[i].loaded = true;
			match = true;
			break;
		}
	}

	// Unable to find the driver that the userspace requested
	if (!match) {
		// Check if this module is built-in without module_init()
		for (i = 0; builtin_list[i]; i++) {
			if (!strcmp(builtin_list[i], modname))
				return;
		}
		__fatal("failed to find a built-in module with the name \"%s\"\n", modname);
		return;
	}

	// Setup args
	if (uargs != NULL) {
		args = strndup_user(uargs, ~0UL >> 1);
		if (IS_ERR(args)) {
			pr_err("failed to parse args: %d\n", PTR_ERR(args));
		} else {
			/*
			 * Parameter parsing is done in two steps for integrated modules
			 * because built-in modules have their parameter names set as
			 * "modname.param", which means that each parameter name in the
			 * arguments must have "modname." prepended to it, or it won't
			 * be found.
			 *
			 * Since parse_args() has a lot of complex logic for actually
			 * parsing out arguments, do parsing in two parse_args() steps.
			 * The first step just makes parse_args() parse out each
			 * parameter/value pair and then pass it to
			 * integrated_module_param_cb(), which builds the correct
			 * parameter name for the built-in module and runs parse_args()
			 * for real. This means that parse_args() recurses, but the
			 * recursion is fixed because integrated_module_param_cb()
			 * passes a different unknown handler,
			 * unknown_integrated_module_param_cb().
			 */
			if (*args)
				parse_args(modname, args, NULL, 0, 0, 0, NULL,
					   integrated_module_param_cb);
		}
	}

	ret = fn();
#ifndef DEBUG
	if (ret != 0)
#endif
		pr_info("lazy_initcalls[%d]: %s's init function returned %d\n", i, modname, ret);

	// Check if all modules are loaded so that __init memory can be released
	match = false;
	for (i = 0; i < counter; i++) {
		if (lazy_initcalls[i].type == NORMAL && !lazy_initcalls[i].loaded)
			match = true;
	}

#ifdef CONFIG_LAZY_INITCALL_DEBUG
	if (!match)
		cancel_delayed_work_sync(&show_unused_work);
	else
		queue_delayed_work(system_freezable_power_efficient_wq,
				&show_unused_work, 5 * HZ);
#endif
	if (!match)
		WRITE_ONCE(completed, true);

	return;
}

static noinline int __init __load_module(struct load_info *info, const char __user *uargs,
		       int flags)
{
	long err;

	/*
	 * Do basic sanity checks against the ELF header and
	 * sections.
	 */
	err = elf_validity_check(info);
	if (err) {
		pr_err("Module has invalid ELF structures\n");
		goto err;
	}

	/*
	 * Everything checks out, so set up the section info
	 * in the info structure.
	 */
	err = setup_load_info(info, flags);
	if (err)
		goto err;

	load_modname(info->name, uargs);

err:
	free_copy(info);
	return err;
}

static int __ref load_module(struct load_info *info, const char __user *uargs,
		       int flags)
{
	int i, ret = 0;

	mutex_lock(&lazy_initcall_mutex);

	if (completed) {
		// Userspace may ask even after all modules have been loaded
		goto out;
	}

	ret = __load_module(info, uargs, flags);
	smp_wmb();

	if (completed) {
		if (deferred_list[0] != NULL) {
			pr_info("all userspace modules loaded, now loading built-in deferred drivers\n");

			for (i = 0; deferred_list[i]; i++)
				load_modname(deferred_list[i], NULL);
		}
		pr_info("all modules loaded, calling free_initmem()\n");
		free_initmem();
		mark_readonly();
	}

out:
	mutex_unlock(&lazy_initcall_mutex);
	return ret;
}

static int may_init_module(void)
{
	if (!capable(CAP_SYS_MODULE))
		return -EPERM;

	return 0;
}

SYSCALL_DEFINE3(init_module, void __user *, umod,
		unsigned long, len, const char __user *, uargs)
{
	int err;
	struct load_info info = { };

	err = may_init_module();
	if (err)
		return err;

	err = copy_module_from_user(umod, len, &info);
	if (err)
		return err;

	return load_module(&info, uargs, 0);
}

SYSCALL_DEFINE3(finit_module, int, fd, const char __user *, uargs, int, flags)
{
	struct load_info info = { };
	void *hdr = NULL;
	int err;

	err = may_init_module();
	if (err)
		return err;

	err = kernel_read_file_from_fd(fd, 0, &hdr, INT_MAX, NULL,
				       READING_MODULE);
	if (err < 0)
		return err;
	info.hdr = hdr;
	info.len = err;

	return load_module(&info, uargs, flags);
}
