#!/bin/sh -e

## Maddes Mini-Debian GRUB release 1 script

for SRC in /root/bin/luks_OLD*.inc ; do . "${SRC}" ; done
[ -z "${OLDROOT}" ] && {
  echo 'ERROR: missing OLDROOT variable'
  printf '\a'
  return 1 2>/dev/null || exit 1
}
[ -z "${OLDGRUB1DEFAULT}" ] && {
  echo 'ERROR: missing OLDGRUB1 default variable'
  printf '\a'
  return 1 2>/dev/null || exit 1
}

for SRC in /root/bin/luks_MINIDEB*.inc ; do . "${SRC}" ; done
[ -z "${MINIDEB}" ] && {
  echo 'ERROR: missing MINIDEB partition variable'
  printf '\a'
  return 1 2>/dev/null || exit 1
}
[ -z "${MINIOLDGRUB1DEFAULT}" ] && {
  echo 'ERROR: missing MINIGRUB1 default variable'
  printf '\a'
  return 1 2>/dev/null || exit 1
}

case "$(pwd)" in
  (/boot|/boot/*) echo 'ERROR: os-prober needs /boot unmounted, leave /boot via cd ${HOME}'
                  return 1 2>/dev/null || exit 1
                  printf '\a'
                  ;;
esac

[ -z "$(which linux-boot-prober 2> /dev/null)" ] && {
  echo 'ERROR: missing linux-boot-prober'
  printf '\a'
  return 1 2>/dev/null || exit 1
}

[ -n "${OLDBOOT}" ] && umount /boot
LINUX="$(linux-boot-prober ${MINIDEBMOUNT} | head -n 1)"
[ -n "${OLDBOOT}" ] && mount -a

[ -z "${LINUX}" ] && {
  echo "ERROR: linux-boot-prober ${MINIDEBMOUNT} failed"
  printf '\a'
  return 1 2>/dev/null || exit 1
}

[ "${OLDGRUB1DEFAULT}" = 'saved' -a "${OLDGRUB1SAVED}" = 'default' ] && {
  echo 'ERROR: GRUB 1 default is set to "saved" and saved value is "default"'
  printf '\a'
  return 1 2>/dev/null || exit 1
}

LROOT="$(echo "${LINUX}" | cut -d ':' -f 1)"
LBOOT="$(echo "${LINUX}" | cut -d ':' -f 2)"
LLABEL='Mini-Debian'  # fix title
LKERNEL="$(echo "${LINUX}" | cut -d ':' -f 4)"
LINITRD="$(echo "${LINUX}" | cut -d ':' -f 5)"
LPARAMS="$(echo "${LINUX}" | cut -d ':' -f 6- | tr '^' ' ')"

# fix problems of linux-boot-prober on Debian 4.0 "Etch"
[ -n "${OLDBOOT}" ] && {
  LKERNEL="$(echo "${LKERNEL}" | sed -e 's#/boot/#/#')"
  LINITRD="$(echo "${LINITRD}" | sed -e 's#/boot/#/#')"
  LPARAMS="$(echo "${LPARAMS}" | sed -e "s#UUID=${OLDROOTUUID}#UUID=${MINIDEBUUID}#" -e "s#\(${OLDROOT}\|${OLDROOTMOUNT}\)#${MINIDEBMOUNT}#")"
}

cat > /boot/grub/custom.mini-debian.oldroot.cfg << __EOF
### BEGIN grub1_mini-debian.sh ###
title		${LLABEL}
# FIXME: set to previous default plus 1 (normally 0 plus 1 = 1)
savedefault	${MINIOLDGRUB1DEFAULT}
# FIXME: set correct value for ${OLDGRUB}, partition is minus 1
root		(${OLDGRUB1DEV},${OLDGRUB1PART})
# FIXME: set correct root value for ${MINIDEBMOUNT}, no /boot prefix if separate boot partition
kernel		${LKERNEL} ${LPARAMS}
# FIXME: no /boot prefix if separate boot partition
initrd		${LINITRD}.mini-debian
### END grub1_mini-debian.sh ###
__EOF
