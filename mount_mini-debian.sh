#!/bin/sh -e

for SRC in /root/bin/luks_MINIDEB*.inc ; do [ ! -s "${SRC}" ] || . "${SRC}" ; done
[ -z "${MINIDEB}" ] && { echo 'ERROR: var MINIDEB not set'; return 1 2>/dev/null || exit 1; }

for SRC in /root/bin/luks_OLD*.inc ; do [ ! -s "${SRC}" ] || . "${SRC}" ; done
[ -z "${OLDROOT}" ] && { echo 'ERROR: var OLDROOT not set'; return 1 2>/dev/null || exit 1; }

for SRC in /root/bin/luks_NEW*.inc ; do [ ! -s "${SRC}" ] || . "${SRC}" ; done

MOUNTBASE='/mnt/mini'

[ -d "${MOUNTBASE}" ] || mkdir "${MOUNTBASE}"

grep -q -F -e "${MOUNTBASE} " /proc/mounts || {
  echo "Mounting ${MINIDEBMOUNT} on ${MOUNTBASE}"
  while mount "${MINIDEBMOUNT}" "${MOUNTBASE}" 2>/dev/null
   do
    :
  done
}

unset BOOTMOUNT
[ "${1}" = 'noboot' ] || {
  BOOTMOUNT="$(eval echo "\${${1:-OLDBOOT}MOUNT}")"
}

grep -q -F -e "${MOUNTBASE}/boot " /proc/mounts && {
  echo "Unmounting ${MOUNTBASE}/boot"
  while umount "${MOUNTBASE}/boot" 2>/dev/null
   do
    :
  done
}

[ -z "${BOOTMOUNT}" ] || {
  grep -q -F -e "${MOUNTBASE}/boot " /proc/mounts || {
    echo "Mounting ${BOOTMOUNT} on ${MOUNTBASE}/boot"
#    mount --bind /boot "${MOUNTBASE}/boot"
    while mount "${BOOTMOUNT}" "${MOUNTBASE}/boot" 2>/dev/null
     do
      :
    done
  }
}

mount_sys.sh "${MOUNTBASE}"

echo "${MOUNTBASE} is set up as following..."
mount | grep -F -e "${MOUNTBASE}"
