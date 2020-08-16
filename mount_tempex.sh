#!/bin/sh -eu

for SRC in /root/bin/luks_TEMPEX*.inc ; do [ ! -s "${SRC}" ] || . "${SRC}" ; done
if [ -z "${TEMPEX:-}" ]; then
  echo 'ERROR: var TEMPEX not set'
  return 1 2>/dev/null || exit 1
fi

. /root/bin/luks_debian.inc

for SRC in /root/bin/luks_func*.inc ; do . "${SRC}" ; done

MOUNTBASE='/mnt/tempex'

[ -d "${MOUNTBASE}" ] || mkdir "${MOUNTBASE}"

start_fs_subdevs TEMPEX

grep -q -F -e "${MOUNTBASE} " /proc/mounts || {
  echo "Mounting ${TEMPEXMOUNT} on ${MOUNTBASE}"
  while mount "${TEMPEXMOUNT}" "${MOUNTBASE}" 2>/dev/null
   do
    :
  done
}

#not needed: boot and sys mounts

echo "${MOUNTBASE} is set up as following..."
mount | grep -F -e "${MOUNTBASE}"
