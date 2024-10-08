#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # Apply patch to scripts/dtc/dtc-lexer.l
    git restore './scripts/dtc/dtc-lexer.l'
    sed -i '41d' './scripts/dtc/dtc-lexer.l'
    
    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper   # deep clean kernel build tree
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig  # set default config
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all    # build kernel image for QEMU booting
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules    # build any kernel modules 
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs       # build device tree
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "############### Creating base directories ###############"
mkdir rootfs && cd rootfs
mkdir usr etc bin sbin dev tmp lib lib64 var home proc sys 
mkdir usr/bin usr/sbin usr/lib
mkdir var/log  

cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
echo "############### Make and install busybox ###############" 
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "############### Adding library dependencies to rootfs ###############"
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp ${SYSROOT}/lib/ld-linux-aarch64.so.1 lib/
cp ${SYSROOT}/lib64/libm.so.6 lib64/
cp ${SYSROOT}/lib64/libresolv.so.2 lib64/
cp ${SYSROOT}/lib64/libc.so.6 lib64/

# TODO: Make device nodes
echo "############### Make device nodes ###############"
cd ${OUTDIR}/rootfs
sudo mknod -m 666 dev/null c 1 3       # creating NULL device with major 1 and minor 3
sudo mknod -m 666 dev/console c 5 1    # creating console device with major 5 and minor 1

# TODO: Clean and build the writer utility
echo "############### Cleaning and building writer utility ###############"
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "############### Copying finder scripts to target ###############"
cp ${FINDER_APP_DIR}/finder.sh $OUTDIR/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh $OUTDIR/rootfs/home/
cp ${FINDER_APP_DIR}/writer.c $OUTDIR/rootfs/home
cp ${FINDER_APP_DIR}/writer $OUTDIR/rootfs/home/
cp ${FINDER_APP_DIR}/autorun-qemu.sh $OUTDIR/rootfs/home/
cp -r ${FINDER_APP_DIR}/conf $OUTDIR/rootfs/home/
cp -r ${FINDER_APP_DIR}/conf/ $OUTDIR/rootfs/

# TODO: Chown the root directory
cd $OUTDIR/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
# Using rootfs with the target
# Using cpio utility to prepare Ramdisk
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
gzip -f initramfs.cpio

echo "############### Done! ###############"