#!/bin/sh -eu

for SRC in /root/bin/luks_OLD*.inc ; do [ ! -s "${SRC}" ] || . "${SRC}" ; done
if [ -z "${OLDROOT:-}" ]; then
  echo 'ERROR: var OLDROOT not set'
  return 1 2>/dev/null || exit 1
fi

. /root/bin/luks_debian.inc

for SRC in /root/bin/luks_func*.inc ; do . "${SRC}" ; done

MOUNTBASE='/mnt/oldroot'

[ -d "${MOUNTBASE}" ] || mkdir "${MOUNTBASE}"

start_fs_subdevs OLDROOT

grep -q -F -e "${MOUNTBASE} " /proc/mounts || {
  echo "Mounting ${OLDROOTMOUNT} on ${MOUNTBASE}"
  while mount "${OLDROOTMOUNT}" "${MOUNTBASE}" 2>/dev/null
   do
    :
  done
}

if [ "${1:-}" = 'all' ]; then
  if [ -n "${OLDBOOT:-}" ]; then
    grep -q -F -e "${MOUNTBASE}/boot " /proc/mounts || {
      echo "Mounting ${OLDBOOTMOUNT} on ${MOUNTBASE}/boot"
      while mount "${OLDBOOTMOUNT}" "${MOUNTBASE}/boot" 2>/dev/null
       do
        :
      done
    }
    grep -q -F -e "${MOUNTBASE}/boot " /proc/mounts || {
      echo "Mounting ${OLDBOOTMOUNT} on ${MOUNTBASE}/boot via bind of /boot"
      mount --bind /boot "${MOUNTBASE}/boot"
    }
  fi

  mount_sys.sh "${MOUNTBASE}"
fi

echo "${MOUNTBASE} is set up as following..."
mount | grep -F -e "${MOUNTBASE}"
