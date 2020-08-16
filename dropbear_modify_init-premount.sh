#!/bin/sh -eu

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

sed -i -e 's#^[[:space:]]*\(/sbin/dropbear\)\([[:space:]]\+\$PKGOPTION_dropbear_OPTION\|\)[[:space:]]*$#\1 ${DROPBEAR_OPTIONS:-$PKGOPTION_dropbear_OPTION}#' "${DESTDIR}/scripts/init-premount/dropbear"
