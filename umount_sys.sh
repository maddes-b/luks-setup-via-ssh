#!/bin/sh -eu

### Check parameters
[ -n "${1:-}" ] || { printf -- 'ERROR: No parameter given\n'; return 1 2>/dev/null || exit 1; }
[ -d "${1}" ] || { printf -- 'ERROR: parameter not a directory\n'; return 1 2>/dev/null || exit 1; }

### Unmount /dev
printf -- 'Unmounting %s/dev\n' "${1}"
RC=0
COUNT=0
while [ "${COUNT}" -lt 10 ]
 do
  COUNT="$(( ${COUNT} + 1 ))"
  for MOUNT in "${1}/dev/mqueue" "${1}/dev/shm" "${1}/dev/pts" "${1}/dev"
   do
    grep -q -F -e "${MOUNT}" /proc/mounts && { umount "${MOUNT}" || true ; } || true
  done
  grep -q -F -e "${1}/dev" /proc/mounts || { RC="${?}" ; break ; }
done
if [ "${RC}" -eq 0 ]; then
  printf -- "Couldn't umount %s/dev/pts %s/dev/shm %s/dev in %s tries\n" "${1}" "${1}" "${1}" "${COUNT}"
  printf -- 'Try to kill related processes (lsof/fuser) or reboot\n' ; printf -- '\a'
fi

### Unmount /proc /sys /run
printf -- 'Unmounting %s/<system mounts>\n' "${1}"
for MOUNT in "${1}/sys" "${1}/proc" "${1}/run/lock" "${1}/run"
 do
  grep -q -F -e "${MOUNT}" /proc/mounts || continue
  printf -- 'Unmounting %s\n' "${MOUNT}"
  while umount "${MOUNT}" 2>/dev/null
   do
    :
  done
done
