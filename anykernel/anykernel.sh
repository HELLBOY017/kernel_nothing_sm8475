# AnyKernel3 Ramdisk Mod Script
# osm0sis @ xda-developers

### AnyKernel setup
# global properties
properties() { '
kernel.string=Meteoric Kernel by HELLBOY017
do.devicecheck=1
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=Pong
device.name2=PongIND
device.name3=PongEEA
'; } # end properties

# boot image installation
block=boot;
is_slot_device=1;
## AnyKernel methods (DO NOT CHANGE)
# import patching functions/variables - see for reference
. tools/ak3-core.sh;
split_boot;
flash_boot;

# vendor_boot installation (for dtb)
block=vendor_boot;
is_slot_device=1;
reset_ak;
split_boot;
flash_boot;

# dtbo installation
flash_generic dtbo;
