#!/bin/sh -e

MOUNTBASE='/mnt/newroot'

umount_sys.sh "${MOUNTBASE}"

echo "Unmounting ${MOUNTBASE}"
for MOUNT in "${MOUNTBASE}/boot" "${MOUNTBASE}"
 do
  grep -q -F -e "${MOUNT}" /proc/mounts || continue
  echo "Unmounting ${MOUNT}"
  while umount "${MOUNT}" 2>/dev/null
   do
    :
  done
done
