// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Juhyung Park <qkrwngud825@gmail.com>
 *
 * Partially based on kernel/module.c.
 */

//#define DEBUG

#define pr_fmt(fmt) "lazy_initcall: " fmt
#define LAZY_INITCALL_INTERNAL

#include <linux/printk.h>
#include <linux/syscalls.h>
#include <linux/kmemleak.h>
#include <uapi/linux/module.h>
#include <uapi/linux/time.h>

#include "module-internal.h"

static void __init show_errors(struct work_struct *unused);
static __initdata DECLARE_DELAYED_WORK(show_errors_work, show_errors);
static DEFINE_MUTEX(lazy_initcall_mutex);
static bool completed;

/*
 * Number of lazy modules.
 *
 * Memory allocated from this is freed later anyways, so a large value
 * is not going to hog the memory down the road.
 */
#define MODULE_LIST_LIMIT 1024

/*
 * Some drivers behave differently when it's built-in, so deferring its
 * initialization causes issues.
 *
 * Put those to this blacklist so that it is ignored from lazy_initcall.
 *
 * You can also use this as an ignorelist.
 */
static const __initconst char * const blacklist[] = {
        // Custom load order
	"hdcp",

	// Disabled
	"f_fs_ipc_log",
	"mem_offline",
	"msm_show_resume_irq",
	"phy_qcom_ufs_qmp_v4_anarok",
	"phy_qcom_ufs_qmp_v4_diwali",
	"phy_qcom_ufs_qmp_v4_lahaina",
	"phy_qcom_ufs_qmp_v4_parrot",
	"phy_qcom_ufs_qmp_v4_waipio",
	"qca6490",
	"qcom_ipc_logging",
	"qcom_logbuf_vendor_hooks",
	"rimps_log",
	"wcd937x_dlkm",
	"wcd937x_slave_dlkm",

	// Renamed
	"hdcp_qseecom",
        
        // Removed
        "fsa4480_i2c",
        "qti_amoled_ecm",
        "nothing_disk_usage",
        "nothing_readahead_trace",
        "nothing_writeback_kmsg",
        "memory_dump_v2",
        "minidump",
        "qcom_edac",
        "kryo_arm64_edac",
        "qnoc_parrot",
        "qcom_cpufreq_hw_debug",
        "qcom_cpuss_sleep_stats",
        "qti_battery_debug",
        "charger_ulog_glink",
        "debug_regulator",
        "qcom_iommu_debug",
        "spmi_pmic_arb_debug",
        "spmi_glink_debug",
        "tz_log",
        "icc_test",
        "icc_debug",
        "coresight",
        "coresight_tmc",
        "coresight_funnel",
        "coresight_replicator",
        "coresight_stm",
        "coresight_cti",
        "coresight_tpda",
        "coresight_tpdm",
        "coresight_csr",
        "coresight_dummy",
        "coresight_remote_etm",
        "coresight_tgu",
        "coresight_hwevent",
        "stm_core",
        "stm_p_basic",
        "stm_p_ost",
        "stm_console",
        "stm_ftrace",
        "qti_regmap_debugfs",
        "dispcc_diwali",
        "gcc_diwali",
        "pinctrl_diwali",
        "qnoc_diwali",
        "gpucc_diwali",
        "camcc_diwali",
        "videocc_diwali",
        "debugcc_diwali",

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

static struct lazy_initcall __initdata lazy_initcalls[MODULE_LIST_LIMIT];
static int __initdata counter;

static char __initdata errors_str[16 * 1024];

#define __err(...) do { \
	size_t len = strlen(errors_str); \
	char *ptr = errors_str + len; \
	snprintf(ptr, sizeof(errors_str) - len, __VA_ARGS__); \
	smp_mb(); \
	pr_err("%s", ptr); \
} while (0)

bool __init add_lazy_initcall(initcall_t fn, char modname[], char filename[])
{
	int i;
	enum lazy_initcall_type type = NORMAL;

	for (i = 0; blacklist[i]; i++) {
		if (!strcmp(blacklist[i], modname))
			return false;
	}

	for (i = 0; deferred_list[i]; i++) {
		if (!strcmp(deferred_list[i], modname)) {
			type = DEFERRED;
			break;
		}
	}

	// Check for duplicates
	for (i = 0; i < counter; i++) {
		if (!strcmp(lazy_initcalls[i].modname, modname)) {
			__err("duplicate module detected: %s - %s\n", modname, filename);
			__err("enable DEBUG from %s to debug further\n", __FILE__);
		}
	}

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

static bool __init show_errors_str(void)
{
	char *s, *p, *tok;

	if (strlen(errors_str) == 0)
		return false;

	s = kstrdup(errors_str, GFP_KERNEL);
	if (!s)
		return true;

	for (p = s; (tok = strsep(&p, "\n")) != NULL; )
		if (tok[0] != '\0')
			pr_err("%s\n", tok);

	kfree(s);

	return true;
}

static void __init show_errors(struct work_struct *unused)
{
	int i;

	// Start printing only after 30s of uptime
	if (ktime_to_us(ktime_get_boottime()) < 30 * USEC_PER_SEC)
		goto out;

	show_errors_str();

	for (i = 0; i < counter; i++) {
		if (!lazy_initcalls[i].loaded) {
			pr_err("lazy_initcalls[%d]: %s not loaded yet\n", i, lazy_initcalls[i].modname);
		}
	}

out:
	queue_delayed_work(system_freezable_power_efficient_wq,
			&show_errors_work, 5 * HZ);
}

static int __init unknown_integrated_module_param_cb(char *param, char *val,
					      const char *modname, void *arg)
{
	__err("%s: unknown parameter '%s' ignored\n", modname, param);
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
		char kmod[MODULE_NAME_LEN + 24];
		void *found;

		snprintf(kmod, sizeof(kmod), "__mod_present__kmod_%s__", modname);
		found = (void *)kallsyms_lookup_name(kmod);
		if (found != NULL)
			return;

		__err("failed to find a built-in module with the name \"%s\"\n", modname);
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
			kfree(args);
		}
	}

	ret = fn();
	if (ret != 0)
		__err("lazy_initcalls[%d]: %s's init function returned %d\n", i, modname, ret);

	// Check if all modules are loaded so that __init memory can be released
	match = false;
	for (i = 0; i < counter; i++) {
		if (lazy_initcalls[i].type == NORMAL && !lazy_initcalls[i].loaded)
			match = true;
	}

	if (!match)
		cancel_delayed_work_sync(&show_errors_work);
	else
		queue_delayed_work(system_freezable_power_efficient_wq,
				&show_errors_work, 5 * HZ);

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
		if (show_errors_str())
			WARN(1, "all modules loaded with errors, review if necessary");
		free_initmem();
		mark_readonly();
	}

out:
	free_copy(info);
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
