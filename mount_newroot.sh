#!/bin/sh -e

for SRC in /root/bin/luks_NEW*.inc ; do [ ! -s "${SRC}" ] || . "${SRC}" ; done
[ -z "${NEWROOT}" ] && { echo 'ERROR: var NEWROOT not set'; return 1 2>/dev/null || exit 1; }

. /root/bin/luks_debian.inc

for SRC in /root/bin/luks_func*.inc ; do . "${SRC}" ; done

MOUNTBASE='/mnt/newroot'

[ -d "${MOUNTBASE}" ] || mkdir "${MOUNTBASE}"

start_fs_subdevs NEWROOT

grep -q -F -e "${MOUNTBASE} " /proc/mounts || {
  echo "Mounting ${NEWROOTMOUNT} on ${MOUNTBASE}"
  while mount "${NEWROOTMOUNT}" "${MOUNTBASE}" 2>/dev/null
   do
    :
  done
}

[ -d "${MOUNTBASE}/boot" ] && {
  [ "${1}" != 'noboot' ] && {
    grep -q -F -e "${MOUNTBASE}/boot " /proc/mounts || {
      echo "Mounting ${NEWBOOTMOUNT} on ${MOUNTBASE}/boot"
      while mount "${NEWBOOTMOUNT}" "${MOUNTBASE}/boot" 2>/dev/null
       do
        :
      done
    }
    grep -q -F -e "${MOUNTBASE}/boot " /proc/mounts || {
      echo "Mounting ${NEWBOOTMOUNT} on ${MOUNTBASE}/boot via bind of /boot"
      mount --bind /boot "${MOUNTBASE}/boot"
    }
  }
  [ "${1}" = 'noboot' ] && grep -q -F -e "${MOUNTBASE}/boot " /proc/mounts && {
    echo "Unmounting ${MOUNTBASE}/boot"
    while umount "${MOUNTBASE}/boot" 2>/dev/null
     do
      :
    done
  }
}

mount_sys.sh "${MOUNTBASE}"

echo "${MOUNTBASE} is set up as following..."
mount | grep -F -e "${MOUNTBASE}"
