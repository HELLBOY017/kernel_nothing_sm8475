# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.

hostprogs	:= ufdt_apply_overlay
always-y	:= $(hostprogs)

# Common objects used by multiple targets
common-objects := ufdt_convert.o ufdt_node.o ufdt_node_pool.o ufdt_overlay.o ufdt_prop_dict.o tests/src/util.o sysdeps/libufdt_sysdeps_posix.o
common-objects := $(addprefix libufdt/,$(common-objects))

# Directly compile in libfdt from dtc-aosp instead of linking to libfdt
libfdt-objs := fdt.o fdt_ro.o fdt_wip.o fdt_sw.o fdt_rw.o fdt_strerror.o \
	       fdt_empty_tree.o fdt_addresses.o fdt_overlay.o fdt_check.o acpi.o
libfdt-objs := $(addprefix ../dtc-aosp/dtc/libfdt/,$(libfdt-objs))

# Sources for ufdt_apply_overlay
ufdt_apply_overlay-objs := libufdt/tests/src/ufdt_overlay_test_app.o $(common-objects) $(libfdt-objs)

# We're working with a submodule, so make these all relative to that.
HOST_EXTRACFLAGS := -I$(srctree)/scripts/dtc-aosp/dtc/libfdt \
		    -I$(srctree)/$(src)/libufdt/sysdeps/include \
                    -I$(srctree)/$(src)/libufdt/include
HOST_EXTRACFLAGS += -std=gnu99 -Wno-missing-prototypes -Wno-strict-prototypes -Wno-format
