/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Sultan Alsawaf <sultan@kerneltoast.com>.
 */

/*
 * WARNING: Do not include this file directly! This file is included for all
 * integrated modules so that all of them have a flag that tracks whether the
 * integrated module is initialized yet or not.
 */

/*
 * Skip objects which are used by multiple modules, and instead only consider
 * objects which are unique to a module.
 */
#ifdef MODULE
#include <linux/init.h>
int8_t __initdata __weak __used __PASTE(__mod_present__, __PASTE(__KBUILD_MODNAME, __));
#endif
