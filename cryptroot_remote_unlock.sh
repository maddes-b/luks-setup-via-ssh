#!/bin/sh -eu

#
# This InitRAMFS hook provides:
# Simple script to easily unlock LUKS encrypted root partition from remote (SSH, Telnet)
# Intended for Debian 6.0 Squeeze
#
# Copyright: Matthias Bücher, see https://www.maddes.net/
# License: GNU GPL v2 or later, see http://www.gnu.org/licenses/gpl.html
#
# Adopted from http://www.howtoforge.com/unlock-a-luks-encrypted-root-partition-via-ssh-on-ubuntu#comment-25990
#
# Thanks to:
# - Wulf Coulmann; http://gpl.coulmann.de/ssh_luks_unlock.html
#   for his tremendeous effort to unlock LUKS root parititon remotely on Debian 5.0 Lenny and before
#
# How to use:
# - Apply patch to /usr/share/initramfs-tools/scripts/local-top/cryptroot
# - Copy this hook script as /etc/initramfs-tools/hooks/cryptroot_remote_unlock.sh
# - chmod +x /etc/initramfs-tools/hooks/cryptroot_remote_unlock.sh
# - update-initramfs -u -k all
#
# History:
# v1.0 - 2011-02-15
#  initial release
# v1.1 - 2011-03-29
#  fixed some typos
#  (also thanks to Sven Greuer)
# v1.2 - 2015-05-24
#  define PATH inside unlock script to make it always work
#

PREREQ=""

prereqs()
{
	printf -- '%s\n' "${PREREQ}"
}

case "${1:-}" in
 prereqs)
	prereqs
	exit 0
	;;
esac

. /usr/share/initramfs-tools/hook-functions

#
# Begin real processing
#

SCRIPTNAME='unlock'

# 1) Create scripts to unlock LUKS partitions and kill all askpass processes
cat > "${DESTDIR}/bin/${SCRIPTNAME}" << __EOF
#!/bin/sh
PATH='/sbin:/bin'
/scripts/local-top/cryptroot remote
cat /conf/conf.d/cryptroot
ls -la /dev/mapper
printf -- 'Call "${SCRIPTNAME}_done" if all is unlocked\n'
__EOF
chmod 700 "${DESTDIR}/bin/${SCRIPTNAME}"

cat > "${DESTDIR}/bin/${SCRIPTNAME}_done" << '__EOF'
#!/bin/sh
PATH='/sbin:/bin'
for PID in $(ps | grep -e '/lib/cryptsetup/askpass' -e 'plymouth.*ask-for-password' | sed -n -e '/grep/! { s#[[:space:]]*\([0-9]\+\)[[:space:]]*.*#\1#p ; }')
 do
	kill -9 "${PID}"
done
printf -- "On Debian 6.0 maybe have to call '${0}' several times\n"
__EOF
chmod 700 "${DESTDIR}/bin/${SCRIPTNAME}_done"

# 2) Enhance Message Of The Day (MOTD) with info how to unlock LUKS partition
cat >> "${DESTDIR}/etc/motd" << __EOF

To unlock root partition, and maybe others like swap, run "${SCRIPTNAME}"
__EOF
