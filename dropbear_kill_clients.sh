#!/bin/sh

#
# This InitRAMFS script provides:
# Simple script to kill all DropBear client sessions if the InitRAMFS is left
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
# - Copy this hook script as /etc/initramfs-tools/scripts/local-bottom/dropbear_kill_clients.sh
# - chmod +x /etc/initramfs-tools/scripts/local-bottom/dropbear_kill_clients.sh
# - update-initramfs -u
#
# History:
# v1.0 - 2011-02-15
#  initial release
# v1.1 - 2011-03-29
#  fixed some typos, thanks to Sven Greuer
#

PREREQ=""

prereqs()
{
	echo "${PREREQ}"
}

case "${1}" in
 prereqs)
	prereqs
	exit 0
	;;
esac

#
# Begin real processing
#

NAME=dropbear
PROG=/sbin/dropbear

# get all server pids that should be ignored
ignore=""
for server in `cat /var/run/${NAME}*.pid`
 do
	ignore="${ignore} ${server}"
done

# get all running pids and kill client connections
for pid in `pidof "${NAME}"`
 do
	# check if correct program, otherwise process next pid
	grep -q -F -e "${PROG}" "/proc/${pid}/cmdline" || {
		continue
	}

	# check if pid should be ignored (servers)
	skip=0
	for server in ${ignore}
	 do
		[ "${pid}" = "${server}" ] && {
			skip=1
			break
		}
	done
	[ "${skip}" -ne 0 ] && continue

	# kill process
	echo "${0}: Killing ${pid}..."
	kill -KILL "${pid}"
done
