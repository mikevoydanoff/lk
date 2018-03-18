#!/usr/bin/env bash

# Copyright 2017 The Fuchsia Authors
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT

set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

MKBOOTIMG="mkbootimg"

MEMBASE=0x00000000
KERNEL_OFFSET=0x00080000
RAMDISK_OFFSET=0x07c00000
DT_OFFSET=0x07a00000

CMDLINE=

function HELP {
    echo "help:"
    echo "-c <cmd line>  : Extra command line options"
    echo "-h for help"
    exit 1
}

while getopts "b:c:d:mh" FLAG; do
    case $FLAG in
        c) CMDLINE+=" ${OPTARG}";;
        h) HELP;;
        \?)
            echo unrecognized option
            HELP
            ;;
    esac
done
shift $((OPTIND-1))

BUILD_DIR=build-hikey960-test
KERNEL="${BUILD_DIR}/lk.bin"
OUT_IMAGE="${BUILD_DIR}/boot.img"
RAMDISK="${BUILD_DIR}/ramdisk.img"

# hikey bootloader requires something to load for the ramdisk,
# so just give it the kernel again to make it happy
cp ${KERNEL} ${RAMDISK}

$MKBOOTIMG \
    --kernel "${KERNEL}" \
    --kernel_offset $KERNEL_OFFSET \
    --ramdisk "${RAMDISK}" \
    --ramdisk_offset $RAMDISK_OFFSET \
    --base $MEMBASE \
    --tags_offset $DT_OFFSET \
    --cmdline "${CMDLINE}" \
-o "${OUT_IMAGE}"

fastboot flash boot "${OUT_IMAGE}"
# Can't guarantee that the target has written image to flash before the
# fastboot command completes, so short delay here before reboot.
sleep 1
fastboot reboot
