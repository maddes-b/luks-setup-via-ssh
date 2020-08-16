#!/bin/sh -eu

### Check parameters
[ -n "${1:-}" ] || { printf -- 'ERROR: No parameter given\n'; return 1 2>/dev/null || exit 1; }
[ -d "${1}" ] || { printf -- 'ERROR: parameter not a directory\n'; return 1 2>/dev/null || exit 1; }

### Mount /dev
if [ -d "${1}/dev" ]; then
  grep -q -F -e "${1}/dev" /proc/mounts || { mount -t devtmpfs /dev "${1}/dev" || mount --bind /dev "${1}/dev" ; }
  grep -q -F -e "${1}/dev/pts" /proc/mounts || mount -t devpts /dev/pts "${1}/dev/pts"
  grep -q -F -e "${1}/dev/shm" /proc/mounts || mount -t tmpfs tmpfs "${1}/dev/shm"
  #mqueue on /dev/mqueue type mqueue (rw,relatime)
fi

### Mount /proc
if [ -d "${1}/proc" ]; then
  grep -q -F -e "${1}/proc" /proc/mounts || mount -t proc /proc "${1}/proc"
fi

### Mount /sys
if [ -d "${1}/sys" ]; then
  grep -q -F -e "${1}/sys" /proc/mounts || mount -t sysfs /sys "${1}/sys"
fi

### Mount /run
if [ -d "${1}/run" ]; then
  grep -q -F -e "${1}/run" /proc/mounts || mount -o bind /run "${1}/run"
  grep -q -F -e "${1}/run/lock" /proc/mounts || mount -o bind /run/lock "${1}/run/lock"
fi
