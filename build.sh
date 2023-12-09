#!/usr/bin/env bash

 #
 # Script For Building Android Kernel
 #

##----------------------------------------------------------##
# Specify Kernel Directory
KERNEL_DIR="$(pwd)"

DEFCONFIG=vendor/meteoric_defconfig
VERSION=V1
ANYKERNEL3_DIR=${KERNEL_DIR}/anykernel/

# Compiler cleanup
COMPILER_CLEANUP=true

# Cleanup
CLEANUP=true

# Github Release Repo
RELEASE_REPO=https://github.com/HELLBOY017/kernel_nothing_sm8475.git

# Files
IMAGE=${KERNEL_DIR}/out/arch/arm64/boot/Image
DTBO=${KERNEL_DIR}/out/arch/arm64/boot/dtbo.img
DTB=${KERNEL_DIR}/out/arch/arm64/boot/dtb

# Verbose Build
VERBOSE=0

# Kernel Version
KERVER=$(make kernelversion)

# Specify Final Zip Name
ZIPNAME=Meteoric

##----------------------------------------------------------##
# Specify Linker
LINKER=ld.lld

##----------------------------------------------------------##
# Specify compiler
COMPILER=neutron

##----------------------------------------------------------##
# Sigint detection
SIGINT_DETECT=0

# Start build
BUILD_START=$(date +"%s")
nocol='\033[0m'
red='\033[0;31m'
green='\033[0;32m'
orange='\033[0;33m'
blue='\033[0;34m'
magenta='\033[0;35m'
cyan='\033[0;36m'

read -p "Include KernelSU? (y/n) " RESP 
if [ "$RESP" = "y" ]; then
    ZIPNAME=Meteoric-KernelSU
    KSU_CONFIG=ksu.config
fi

# Clone ToolChain
function cloneTC() {
    if [ $COMPILER = "proton" ]; then
        if [ $COMPILER_CLEANUP = true ]; then
            rm -rf ~/meteoric/neutron-clang
        fi
        git clone --depth=1  https://github.com/kdrag0n/proton-clang.git ~/meteoric/proton-clang
        PATH="${HOME}/meteoric/proton-clang/bin:$PATH"    
    elif [ $COMPILER = "neutron" ]; then
        if [ $COMPILER_CLEANUP = true ]; then
            rm -rf ~/meteoric/proton-clang
        fi
        if [ -d ${HOME}/meteoric/neutron-clang ]; then
            PATH="${HOME}/meteoric/neutron-clang/bin:$PATH"
        else
            mkdir -p ~/meteoric/neutron-clang
            cd ~/meteoric/neutron-clang || exit
            curl -LO "https://raw.githubusercontent.com/Neutron-Toolchains/antman/main/antman"
            chmod a+x antman
            ./antman -S
            cd - || exit
        fi
    fi
    if [ ! -d $ANYKERNEL3_DIR ]; then
        git clone https://github.com/osm0sis/AnyKernel3.git anykernel
    fi
}
	
##------------------------------------------------------##
# Export Variables
function exports() {
    # Export KBUILD_COMPILER_STRING
    export KBUILD_COMPILER_STRING=$(${HOME}/meteoric/$COMPILER-clang/bin/clang --version | head -n 1 | perl -pe 's/\(http.*?\)//gs' | sed -e 's/  */ /g' -e 's/[[:space:]]*$//')
        
    # Export ARCH and SUBARCH
    export ARCH=arm64
    export SUBARCH=arm64
               
    # KBUILD HOST and USER
    export KBUILD_BUILD_HOST=Neoteric
    export KBUILD_BUILD_USER="HELLBOY017"
        
    export PROCS=$(nproc --all)
    export DISTRO=$(source /etc/os-release && echo "${NAME}")
}

function sigint() {
    SIGINT_DETECT=1
}
       
##----------------------------------------------------------##
# Compilation
function compile() {
    # Startup
    echo -e "$cyan***********************************************"
    echo "          STARTING THE ENGINE         "
    echo -e "***********************************************$nocol"
        
    if [ $CLEANUP="true" ]; then
        rm -rf out/*.zip
    fi
        
    read -p "Do you want to do a clean build? (y/n) " RESP 
    if [ "$RESP" = "y" ]; then 
        make O=out clean && make O=out mrproper
    fi
	
    # Compile
    echo "**** Kernel defconfig is set to $DEFCONFIG ****"
    echo -e "$green***********************************************"
    echo "          BUILDING METEORIC KERNEL          "
    echo -e "***********************************************$nocol"
	
    make O=out CC=clang ARCH=arm64 $DEFCONFIG $KSU_CONFIG savedefconfig
    make -kj$(nproc --all) O=out \
    ARCH=arm64 \
    LLVM=1 \
    LLVM_IAS=1 \
    CLANG_TRIPLE=aarch64-linux-gnu- \
    CROSS_COMPILE=aarch64-linux-android- \
    CROSS_COMPILE_COMPAT=arm-linux-androideabi- \
    AR=llvm-ar \
    NM=llvm-nm \
    OBJCOPY=llvm-objcopy \
    OBJDUMP=llvm-objdump \
    STRIP=llvm-strip \
    V=$VERBOSE 2>&1 | tee out/error.log

    # KernelSU
    if [ $ZIPNAME = "Meteoric-KernelSU" ]; then
        sed -i 's/CONFIG_KERNELSU=y/# CONFIG_KERNELSU is not set/g' out/.config
        sed -i '/CONFIG_KERNELSU=y/d' out/defconfig
        
        if [ $(grep -c "# KernelSU" arch/arm64/configs/$DEFCONFIG) -eq 1 ]; then
            sed -i 's/CONFIG_KERNELSU=y/# CONFIG_KERNELSU is not set/g' arch/arm64/configs/$DEFCONFIG
        elif [ $(grep -c "# KernelSU" arch/arm64/configs/$DEFCONFIG) -eq 0 ]; then    
            sed -i '/CONFIG_KERNELSU=y/d' arch/arm64/configs/$DEFCONFIG
        fi
    fi   

    # Verify build
    if [ $(grep -c "Error 2" out/error.log) -ne 0 ] || [ $SIGINT_DETECT -eq 1 ]]; then 
        echo -e ""
        echo -e "$red***********************************************"
        echo "          BUILD ABORTED/UNSUCCESSFUL         "
        echo -e "***********************************************$nocol"
        exit 1
    else
        echo -e "$green*****************************************************"
        echo "    KERNEL COMPILATION FINISHED, STARTING ZIPPING         "
        echo -e "*****************************************************$nocol"   
    fi

    SIGINT_DETECT=0
}

# Zipname
FINAL_ZIP=$ZIPNAME-$VERSION-$(date +%y%m%d-%H%M).zip

function zipping() {
    # Anykernel 3 time!!
    echo "**** Verifying AnyKernel3 Directory ****"
    ls $ANYKERNEL3_DIR
    echo "**** Building anykernel script ****"
    echo "# AnyKernel3 Ramdisk Mod Script
# osm0sis @ xda-developers

## AnyKernel setup
# begin properties
properties() { '
kernel.string=Meteoric $VERSION by HELLBOY017
do.devicecheck=1
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=Pong
device.name2=PongIND
device.name3=PongEEA
supported.versions=
supported.patchlevels=
'; } # end properties

# shell variables
block=boot;
is_slot_device=1;
ramdisk_compression=auto;
patch_vbmeta_flag=auto;

## AnyKernel methods (DO NOT CHANGE)
# import patching functions/variables - see for reference
. tools/ak3-core.sh;

## AnyKernel install
dump_boot;

write_boot;
## end install" > $ANYKERNEL3_DIR/anykernel.sh

    echo "**** Removing leftovers ****"
    rm -rf $ANYKERNEL3_DIR/Image
    rm -rf $ANYKERNEL3_DIR/Image.gz
    rm -rf $ANYKERNEL3_DIR/dtbo.img
    rm -rf  $ANYKERNEL3_DIR/dtb
    rm -rf $ANYKERNEL3_DIR/*.zip

    echo "**** Copying Image & dtbo.img ****"
    cp $IMAGE $ANYKERNEL3_DIR/
    cp $DTBO $ANYKERNEL3_DIR/
    cp $DTB $ANYKERNEL3_DIR/

    echo -e "$magenta***********************************************"
    echo "          Time to zip up!          "
    echo -e "***********************************************$nocol"
    # Make zip and transfer it to kernel directory
    cd $ANYKERNEL3_DIR/
    zip -r9 "../out/$FINAL_ZIP" * -x README $FINAL_ZIP
    
    # Clean AnyKernel3 directory
    cd ..
    rm -rf $ANYKERNEL3_DIR/*.zip
    rm -rf $ANYKERNEL3_DIR/Image
    rm -rf $ANYKERNEL3_DIR/Image.gz
    rm -rf $ANYKERNEL3_DIR/dtbo.img
    rm -rf  $ANYKERNEL3_DIR/dtb
    rm -rf $ANYKERNEL3_DIR/modules/vendor/lib/modules/*
    rm -rf $ANYKERNEL3_DIR/modules/vendor_dlkm/lib/modules/*

    echo -e "$orange***********************************************"
    echo "         Done, here is your sha1         "
    echo -e "***********************************************$nocol"
    sha1sum out/$FINAL_ZIP

    # Github release
    read -p "Do you want to do a github release? (y/n) " RESP 
    if [ "$RESP" = "y" ]; then 
        gh release create $VERSION out/$FINAL_ZIP --repo $RELEASE_REPO --title Meteoric-$VERSION
    else
        read -p "Do you want to upload files to the current github release? (y/n) " RESP 
        if [ "$RESP" = "y" ]; then 
            gh release upload $VERSION out/$FINAL_ZIP --repo $RELEASE_REPO
            echo -e "$cyan***********************************************"
            echo "                All done !!!         "
            echo -e "***********************************************$nocol"
        else 
            echo -e "$cyan***********************************************"
            echo "                All done !!!         "
            echo -e "***********************************************$nocol"
        fi
    fi
}

##----------------------------------------------------------##

cloneTC
exports
trap sigint SIGINT
compile
BUILD_END=$(date +"%s")
DIFF=$(($BUILD_END - $BUILD_START))
echo -e "$blue Build completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds.$nocol"
zipping

##----------------------------------------------------------##
