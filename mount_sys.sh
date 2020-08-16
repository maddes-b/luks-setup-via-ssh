#!/bin/sh -e

### Check parameters
[ -z "${1}" ] && { echo 'ERROR: No parameter given'; return 1 2>/dev/null || exit 1; }
[ ! -d "${1}" ] && { echo 'ERROR: parameter not a directory'; return 1 2>/dev/null || exit 1; }

### Mount /dev
[ ! -d "${1}/dev" ] || {
  grep -q -F -e "${1}/dev" /proc/mounts || { mount -t devtmpfs /dev "${1}/dev" || mount --bind /dev "${1}/dev" ; }
  grep -q -F -e "${1}/dev/pts" /proc/mounts || mount -t devpts /dev/pts "${1}/dev/pts"
  grep -q -F -e "${1}/dev/shm" /proc/mounts || mount -t tmpfs tmpfs "${1}/dev/shm"
  #mqueue on /dev/mqueue type mqueue (rw,relatime)
}

### Mount /proc
[ ! -d "${1}/proc" ] || {
  grep -q -F -e "${1}/proc" /proc/mounts || mount -t proc /proc "${1}/proc"
}

### Mount /sys
[ ! -d "${1}/sys" ] || { 
  grep -q -F -e "${1}/sys" /proc/mounts || mount -t sysfs /sys "${1}/sys"
}

### Mount /run
[ ! -d "${1}/run" ] || { 
  grep -q -F -e "${1}/run" /proc/mounts || mount -o bind /run "${1}/run"
  grep -q -F -e "${1}/run/lock" /proc/mounts || mount -o bind /run/lock "${1}/run/lock"
}
