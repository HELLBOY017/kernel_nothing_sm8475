# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
#
# This is based on the original scripts/dtc/Makefile, but has been revised to
# work with an insane submodule situation.

hostprogs	:= dtc-aosp fdtget fdtput fdtoverlay fdtoverlaymerge 
always-y	:= $(hostprogs)

# Sources for dtc
dtc-aosp-objs	:= dtc.o flattree.o fstree.o data.o livetree.o treesource.o \
		   srcpos.o checks.o util.o
dtc-aosp-objs	+= dtc-lexer.lex.o dtc-parser.tab.o
dtc-aosp-objs	:= $(addprefix dtc/,$(dtc-aosp-objs))

# Sources for libfdt
libfdt-objs := fdt.o fdt_ro.o fdt_wip.o fdt_sw.o fdt_rw.o fdt_strerror.o \
	       fdt_empty_tree.o fdt_addresses.o fdt_overlay.o fdt_check.o acpi.o
libfdt-objs := $(addprefix dtc/libfdt/,$(libfdt-objs))

# Sources for fdtget, fdtput, fdtoverlay and fdtoverlaymerge
fdtget-objs := fdtget.o util.o
fdtget-objs := $(addprefix dtc/,$(fdtget-objs))
fdtget-objs += $(libfdt-objs)

fdtput-objs := fdtput.o util.o
fdtput-objs := $(addprefix dtc/,$(fdtput-objs))
fdtput-objs += $(libfdt-objs)

fdtoverlay-objs := fdtoverlay.o util.o
fdtoverlay-objs	:= $(addprefix dtc/,$(fdtoverlay-objs))
fdtoverlay-objs += $(libfdt-objs)

fdtoverlaymerge-objs := fdtoverlaymerge.o util.o
fdtoverlaymerge-objs := $(addprefix dtc/,$(fdtoverlaymerge-objs))
fdtoverlaymerge-objs += $(libfdt-objs)

# We're working with a submodule, so make these all relative to that.
HOST_EXTRACFLAGS := -I$(srctree)/$(src)/dtc -I$(srctree)/$(src)/dtc/libfdt
HOST_EXTRACFLAGS += -DNO_YAML

# Dependencies on generated files need to be listed explicitly.
$(obj)/dtc/dtc-lexer.lex.o: $(obj)/dtc/dtc-parser.h

# Make the header output .h instead of .tab.h for bison, because aosp is broken.
%.h: %.y
	$(call if_changed,bison)

# Generated files need to be cleaned explicitly.
clean-files	:= dtc/dtc-lexer.lex.c dtc/dtc-parser.tab.c dtc/dtc-parser.h
