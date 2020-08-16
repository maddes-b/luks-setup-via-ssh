#!/bin/sh -eu

## Maddes Mini-Debian GRUB release 2 script

if [ -z "${1:-}" ]; then
  echo 'ERROR: No parameter given'
  return 1 2>/dev/null || exit 1
fi

[ "${1}" = 'oldroot' -o "${1}" = 'mini' ] || { echo 'ERROR: parameter is not "oldroot" or "mini"'; return 1 2>/dev/null || exit 1; }

echo 'NOTE: Mini-Debian must be mounted to work correctly'

prefix='/usr'
exec_prefix="${prefix}"
libdir="${exec_prefix}/lib"

. "${libdir}/grub/grub-mkconfig_lib"

for SRC in /root/bin/luks_OLD*.inc ; do . "${SRC}" ; done
if [ -z "${OLDROOT:-}" ]; then
  echo 'missing OLDROOT variable'
  return 1 2>/dev/null || exit 1
fi

for SRC in /root/bin/luks_MINIDEB*.inc ; do . "${SRC}" ; done
if [ -z "${MINIDEB:-}" ]; then
  echo 'missing MINIDEB variable'
  return 1 2>/dev/null || exit 1
fi

if [ -z "$(which linux-boot-prober 2> /dev/null)" ]; then
  echo 'missing linux-boot-prober'
  return 1 2>/dev/null || exit 1
fi

LINUX="$(linux-boot-prober ${MINIDEB} | head -n 1)"
if [ -z "${LINUX}" ]; then
  echo "linux-boot-prober ${MINIDEB} failed"
  return 1 2>/dev/null || exit 1
fi

LROOT="$(echo "${LINUX}" | cut -d ':' -f 1)"
LBOOT="$(echo "${LINUX}" | cut -d ':' -f 2)"
LLABEL='Mini-Debian'  # fix title
LKERNEL="$(echo "${LINUX}" | cut -d ':' -f 4)"
LINITRD="$(echo "${LINUX}" | cut -d ':' -f 5)"
LPARAMS="$(echo "${LINUX}" | cut -d ':' -f 6- | tr '^' ' ')"

if [ "${1}" = 'oldroot' ]; then
  LBOOT="${OLDBOOT:-${OLDROOT}}"
else
  LBOOT="${OLDBOOT:-${MINIDEB}}"
fi

if [ -n "${OLDBOOT}" ]; then
  LKERNEL="${LKERNEL#/boot}"
  LINITRD="${LINITRD#/boot}"
fi

cat > /boot/grub/custom.cfg << __EOF
### BEGIN grub2_mini-debian.sh (for ${1}) ###
menuentry "${LLABEL}" {
__EOF

if [ "${1}" = 'oldroot' ]; then
  cat >> /boot/grub/custom.cfg << __EOF
# Make sure that the typical default 0 is booted next time...
	set saved_entry=0
${OLDGRUBNOWRITE:+#}	save_env saved_entry
	unset prev_saved_entry
${OLDGRUBNOWRITE:+#}	save_env prev_saved_entry
# ... or directly on problems
	set fallback=0
# define Mini-Debian boot entry
__EOF
fi

save_default_entry | sed -e "s/^/\t/" >>/boot/grub/custom.cfg

prepare_boot_cache="$(prepare_grub_to_access_device ${LBOOT} | sed -e "s/^/\t/")"
printf -- '%s\n' "${prepare_boot_cache}" >>/boot/grub/custom.cfg

cat >> /boot/grub/custom.cfg << __EOF
	echo	'Loading ${LLABEL} (${1}) ...'
	linux	${LKERNEL} ${LPARAMS}
__EOF
if [ -n "${LINITRD}" ]; then
	cat >> /boot/grub/custom.cfg << __EOF
	echo	'Loading initial ramdisk ...'
	initrd	${LINITRD}.mini-debian
__EOF
fi
cat >> /boot/grub/custom.cfg << __EOF
}
### END grub2_mini-debian.sh (for ${1}) ###
__EOF
