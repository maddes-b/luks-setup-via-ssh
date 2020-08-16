#!/bin/sh -e

### Check parameters
[ -z "${1}" ] && { echo 'ERROR: No parameter given'; return 1 2>/dev/null || exit 1; }
[ ! -d "${1}" ] && { echo 'ERROR: parameter not a directory'; return 1 2>/dev/null || exit 1; }

### Unmount /dev
echo "Unmounting ${1}/dev"
RC=0
COUNT=0
while [ "${COUNT}" -lt 10 ]
 do
  COUNT="$(( ${COUNT} + 1 ))"
  for MOUNT in "${1}/dev/mqueue" "${1}/dev/shm" "${1}/dev/pts" "${1}/dev"
   do
    grep -q -F -e "${MOUNT}" /proc/mounts && { umount "${MOUNT}" || true ; }
  done
  grep -q -F -e "${1}/dev" /proc/mounts || { RC="${?}" ; break ; }
done
[ "${RC}" -eq 0 ] && {
  echo "Couldn't umount ${1}/dev/pts ${1}/dev/shm ${1}/dev in ${COUNT} tries"
  echo "Try to kill related processes (lsof/fuser) or reboot" ; printf '\a'
}

### Unmount /proc /sys /run
echo "Unmounting ${1}/<system mounts>"
for MOUNT in "${1}/sys" "${1}/proc" "${1}/run/lock" "${1}/run"
 do
  grep -q -F -e "${MOUNT}" /proc/mounts || continue
  echo "Unmounting ${MOUNT}"
  while umount "${MOUNT}" 2>/dev/null
   do
    :
  done
done
