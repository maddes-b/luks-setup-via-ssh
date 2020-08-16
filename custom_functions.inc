#!/bin/false

printf -- 'Setting functions...\n' 1>&2

[ -f '/etc/debian_version' ] || { printf -- 'Not a Debian system\n'; return 1 2>/dev/null || exit 1; }

CUSTOMSETUPFILES='/home/root/setup_files'
#
unset -v APTVERSION
[ ! -s '/etc/custom/debian' ] || . /etc/custom/debian
DEBSIDRELEASE='12'
#
STANDARDIFS="$(printf -- ' \t\nX')" ; STANDARDIFS="${STANDARDIFS%X}"
NEWLINEIFS="$(printf -- '\nX')" ; NEWLINEIFS="${NEWLINEIFS%X}"

### Distribution
debversion_to_release () {
  case "${DEBVERSION}" in
   ('3')  DEBRELEASE='sarge' ;;
   ('4')  DEBRELEASE='etch' ;;
   ('5')  DEBRELEASE='lenny' ;;
   ('6')  DEBRELEASE='squeeze' ;;
   ('7')  DEBRELEASE='wheezy' ;;
   ('8')  DEBRELEASE='jessie' ;;
   ('9')  DEBRELEASE='stretch' ;;
   ('10') DEBRELEASE='buster' ;;
   ('11') DEBRELEASE='bullseye' ;;
   (${DEBSIDRELEASE}) DEBRELEASE='sid' ;;
  esac

  return 0
}

debrelease_to_version () {
  case "${DEBRELEASE}" in
   ('sarge')    DEBVERSION='3' ;;
   ('etch')     DEBVERSION='4' ;;
   ('lenny')    DEBVERSION='5' ;;
   ('squeeze')  DEBVERSION='6' ;;
   ('wheezy')   DEBVERSION='7' ;;
   ('jessie')   DEBVERSION='8' ;;
   ('stretch')  DEBVERSION='9' ;;
   ('buster')   DEBVERSION='10' ;;
   ('bullseye') DEBVERSION='11' ;;
   ('sid')      DEBVERSION="${DEBSIDRELEASE}" ;;
  esac

  return 0
}

unset -v DIST_NAME
unset -v DIST_NAME_LOWER
unset -v DIST_VERSION
unset -v DIST_MAJOR
unset -v DIST_MINOR
unset -v DIST_MICRO
unset -v DEBRELEASE
RC=0
which lsb_release >/dev/null || RC="${?}"
if [ "${RC}" -eq 0 ] ; then
  DIST_NAME="$(lsb_release -i -s)"
  grep -q -F -e 'OSMC' /etc/issue.net && DIST_NAME='OSMC' || true
  DIST_VERSION="$(lsb_release -r -s)"
  DEBRELEASE="$(lsb_release -c -s)"
  [ "${DIST_NAME}" != 'Debian' -o "${DIST_VERSION}" != 'testing' ] || { debrelease_to_version; DIST_VERSION="${DEBVERSION}.0" ; }
  [ "${DIST_NAME}" != 'Debian' -o "${DEBRELEASE}" != 'sid' ] || DIST_VERSION="${DEBSIDRELEASE}.0"
else
  DIST_NAME='Debian'
  grep -q -F -e 'Ubuntu' /etc/issue.net && DIST_NAME='Ubuntu' || true
  grep -q -F -e 'OSMC' /etc/issue.net && DIST_NAME='OSMC' || true
  grep -q -F -e 'Raspbian' /etc/issue.net && DIST_NAME='Raspbian' || true
  case "${DIST_NAME}" in
    ('Ubuntu') DIST_VERSION="$(sed -n -e 's#[^0-9]*\([0-9.]*\)#\1#p' /etc/issue.net)" ;;
    (*) DIST_VERSION="$(cat /etc/debian_version)"
        DIST_VERSION="$(printf -- '%s' "${DIST_VERSION}" | sed 's#.*/\(sid\)#\1#')"
        [ "${DIST_NAME}" != 'Debian' -o "${DIST_VERSION}" != 'sid' ] || DIST_VERSION="${DEBSIDRELEASE}.0"
        ;;
  esac
fi
[ -z "${DIST_NAME:-}" ] || DIST_NAME_LOWER="$(printf -- '%s' "${DIST_NAME}" | sed 's#.*#\L&#')"

DIST_MAJOR="$(printf -- '%s' "${DIST_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
DIST_MINOR="$(printf -- '%s' "${DIST_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
DIST_VERSION="$(( ${DIST_MAJOR:-0} * 1000000 + ${DIST_MINOR:-0} * 1000 + ${DIST_MICRO:-0} ))"

DEBIAN=0
UBUNTU=0
OSMC=0
RASPBIAN=0
SPARKY=0
case "${DIST_NAME:-}" in
 ('Ubuntu')
  UBUNTU=1
  case "${DIST_VERSION}" in
   (15004000)
    DEBVERSION='8'
    DEBVERSION='7'
    [ -n "${DEBRELEASE:-}" ] || DEBRELEASE='vivid'
    ;;
   (15010000)
    DEBVERSION='8'
    [ -n "${DEBRELEASE:-}" ] || DEBRELEASE='wily'
    ;;
   (16004000)
    DEBVERSION='8'
    [ -n "${DEBRELEASE:-}" ] || DEBRELEASE='xenial'
    ;;
  esac
  ;;
 ('OSMC')
  OSMC=1
  DEBVERSION="${DIST_MAJOR}"
  [ -n "${DEBRELEASE:-}" ] || debversion_to_release
  ;;
 ('Raspbian')
  RASPBIAN=1
  DEBVERSION="${DIST_MAJOR}"
  [ -n "${DEBRELEASE:-}" ] || debversion_to_release
  ;;
 ('Sparky')
  SPARKY=1
  DEBIAN=1
  DEBVERSION="$(cat /etc/debian_version)"
  DEBVERSION="$(printf -- '%s' "${DEBVERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  DEBRELEASE='testing'
  APTVERSION='sparky'
  ;;
 (*)
  DEBIAN=1
  DEBVERSION="${DIST_MAJOR}"
  [ -n "${DEBRELEASE:-}" ] || debversion_to_release
  ;;
esac
[ -n "${APTVERSION:-}" ] || APTVERSION="${DEBVERSION}"

[ -n "${HOSTNAME:-}" ] || HOSTNAME="$(hostname)"
[ -n "${HOSTNAME:-}" -o ! -f '/etc/hostname' ] || HOSTNAME="$(cat /etc/hostname)"

MAXDEBVERSION="$(( ${DEBVERSION:-0} + 5 ))"
[ "${MAXDEBVERSION}" -ge 11 ] || MAXDEBVERSION='11'
DEBVERSIONS="$(seq 1 ${MAXDEBVERSION})"

unset -v CPNOCLOBBER
[ "${DEBVERSION}" -lt 6 ] || CPNOCLOBBER='-n'

### Linux Kernel
## Debian 4.x: 2.6.18-4-amd64
## Debian 5.x: 2.6.26-1-amd64
## Debian 6.x: 2.6.32-5-amd64
## Debian 7.x: 3.2.0-4-amd64
## Debian 8.x: 3.16.0-4-amd64 (ip6tables: module rpfilter, table nat)
## Debian 9.x: 4.9.0-3-amd64
## Debian 10.x: 4.19.0-5-amd64
unset -v LINUX_VERSION
unset -v LINUX_MAJOR
unset -v LINUX_MINOR
unset -v LINUX_MICRO
unset -v LINUX_ARCH
which uname >/dev/null && {
  LINUX_VERSION="$(uname -r)"
  LINUX_MAJOR="$(printf -- '%s' "${LINUX_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  LINUX_MINOR="$(printf -- '%s' "${LINUX_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  LINUX_MICRO="$(printf -- '%s' "${LINUX_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  LINUX_ARCH="$(printf -- '%s' "${LINUX_VERSION}" | grep -o -e '[^-]*$')"
  LINUX_VERSION="$(( ${LINUX_MAJOR:-0} * 1000000 + ${LINUX_MINOR:-0} * 1000 + ${LINUX_MICRO:-0} ))"
}

### IPTABLES
## Debian 4.x: 1.3.6
## Debian 5.x: 1.4.2
## Debian 6.x: 1.4.8
## Debian 7.x: 1.4.14 (ip6tables: module tproxy)
## Debian 8.x: 1.4.21
## Debian 9.x: 1.6.0
## Debian 10.x: 1.8.2 (nf_tables)
unset -v IPTABLES_VERSION
unset -v IPTABLES_MAJOR
unset -v IPTABLES_MINOR
unset -v IPTABLES_MICRO
which iptables >/dev/null && {
  IPTABLES_VERSION="$(iptables --version | sed -n -e 's#[^0-9]*\([0-9.]*\).*#\1#p')"
  IPTABLES_MAJOR="$(printf -- '%s' "${IPTABLES_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  IPTABLES_MINOR="$(printf -- '%s' "${IPTABLES_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  IPTABLES_MICRO="$(printf -- '%s' "${IPTABLES_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  IPTABLES_VERSION="$(( ${IPTABLES_MAJOR:-0} * 1000000 + ${IPTABLES_MINOR:-0} * 1000 + ${IPTABLES_MICRO:-0} ))"
}

### BLKID
## Debian 4.x: 1.0.0
## Debian 5.x: 1.0.0
## Debian 6.x: 2.17.2
## Debian 7.x: 2.20.1 (-o export)
## Debian 8.x: 2.25.2
## Debian 9.x: 2.29.2
## Debian 10.x: 2.33.1
unset -v BLKID_VERSION
unset -v BLKID_MAJOR
unset -v BLKID_MINOR
unset -v BLKID_MICRO
which blkid >/dev/null && {
  BLKID_VERSION="$(blkid -v | sed -n -e 's#[^0-9]*\([0-9.]*\).*#\1#p')"
  BLKID_MAJOR="$(printf -- '%s' "${BLKID_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  BLKID_MINOR="$(printf -- '%s' "${BLKID_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  BLKID_MICRO="$(printf -- '%s' "${BLKID_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  BLKID_VERSION="$(( ${BLKID_MAJOR:-0} * 1000000 + ${BLKID_MINOR:-0} * 1000 + ${BLKID_MICRO:-0} ))"
}

### MDADM
## Debian 4.x: 2.5.6
## Debian 5.x: 2.6.7.2 (--wait)
## Debian 6.x: 3.1.4 (--export)
## Debian 7.x: 3.2.5
## Debian 8.x: 3.3.2 (--export: MD_DEVICE_*_DEV)
## Debian 9.x: 3.4
## Debian 10.x: 4.1
unset -v MDADM_VERSION
unset -v MDADM_MAJOR
unset -v MDADM_MINOR
unset -v MDADM_MICRO
which mdadm >/dev/null && {
  MDADM_VERSION="$(mdadm --version 2>&1 | sed -n -e 's#mdadm - v\(.*\) - .*#\1#p')"
  MDADM_MAJOR="$(printf -- '%s' "${MDADM_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  MDADM_MINOR="$(printf -- '%s' "${MDADM_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  MDADM_MICRO="$(printf -- '%s' "${MDADM_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  MDADM_VERSION="$(( ${MDADM_MAJOR:-0} * 1000000 + ${MDADM_MINOR:-0} * 1000 + ${MDADM_MICRO:-0} ))"
}

### LVM (version / library / driver)
## Debian 4.x: 2.02.07 / 1.02.08 / 4.7.0
## Debian 5.x: 2.02.39 / 1.02.27 / 4.13.0 (--rows --nameprefixes)
## Debian 6.x: 2.02.66 / 1.02.48 / 4.15.0
## Debian 7.x: 2.02.95 / 1.02.74 / 4.22.0
## Debian 8.x: 2.02.111 / 1.02.90 / 4.27.0
## Debian 9.x: 2.02.168 / 1.02.137 / 4.35.0
## Debian 10.x: 2.03.02 / 1.02.155 / 4.39.0
unset -v LVM_VERSION
unset -v LVM_MAJOR
unset -v LVM_MINOR
unset -v LVM_MICRO
which lvm >/dev/null && {
  LVM_VERSION="$(lvm version | sed -n -e 's#[[:space:]]*LVM version:[[:space:]]*\([0-9.]*\).*#\1#p')"
  LVM_MAJOR="$(printf -- '%s' "${LVM_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  LVM_MINOR="$(printf -- '%s' "${LVM_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  LVM_MICRO="$(printf -- '%s' "${LVM_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  LVM_VERSION="$(( ${LVM_MAJOR:-0} * 1000000 + ${LVM_MINOR:-0} * 1000 + ${LVM_MICRO:-0} ))"
}

### LUKS/cryptsetup
## Debian 4.x: 1.0.5
## Debian 5.x: 1.0.6
## Debian 6.x: 1.1.3
## Debian 7.x: 1.4.3
## Debian 8.x: 1.6.6
## Debian 9.x: 1.7.3
## Debian 10.x: 2.1.0
unset -v LUKS_VERSION
unset -v LUKS_MAJOR
unset -v LUKS_MINOR
unset -v LUKS_MICRO
which cryptsetup >/dev/null && {
  LUKS_VERSION="$(cryptsetup --version | sed -n -e 's#[^0-9]*\([0-9.]*\).*#\1#p')"
  LUKS_MAJOR="$(printf -- '%s' "${LUKS_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  LUKS_MINOR="$(printf -- '%s' "${LUKS_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  LUKS_MICRO="$(printf -- '%s' "${LUKS_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  LUKS_VERSION="$(( ${LUKS_MAJOR:-0} * 1000000 + ${LUKS_MINOR:-0} * 1000 + ${LUKS_MICRO:-0} ))"
}

### Command to restart service
unset -v RESTARTCMD1
unset -v RESTARTCMD2
RESTARTCMD1="$(which systemctl)" && { RESTARTCMD1="${RESTARTCMD1} restart"; RESTARTCMD2=''; }
#
if [ -z "${RESTARTCMD1:-}" ]; then
  RESTARTCMD1="$(which service)" && RESTARTCMD2='restart'
fi
#
if [ -z "${RESTARTCMD1:-}" ]; then
  RESTARTCMD1="$(which invoke-rc.d)" && RESTARTCMD2='restart'
fi

### base64 replacement
if [ "${DEBVERSION}" -le 4 ]; then
  base64 () {
    if [ "${1:-}" = '-d' ]; then
      perl -MMIME::Base64 -ne 'print decode_base64($_);'
    else
      perl -MMIME::Base64 -ne 'print encode_base64($_);'
    fi
  }
fi

eval_blkid () {
  unset -v UUID
  unset -v UUID_SUB
  unset -v LABEL
  unset -v TYPE

  if [ "${BLKID_VERSION}" -lt 2020001 ] ; then
    eval "$(blkid -o full ${1} 2>/dev/null | sed -n -e 's#^.*: ## ; s#\([^[:space:]]*="[^"]*"\)[[:space:]]*#\1\n#g ; p' | grep -e '^[[:space:]]*UUID=' -e '^[[:space:]]*UUID_SUB=' -e '^[[:space:]]*LABEL=' -e '^[[:space:]]*TYPE=')"
  else
    eval "$(blkid -o export ${1} 2>/dev/null | grep -e '^[[:space:]]*UUID=' -e '^[[:space:]]*UUID_SUB=' -e '^[[:space:]]*LABEL=' -e '^[[:space:]]*TYPE=')"
  fi

  return 0
}

eval_mdadm () {
  unset -v MD_LEVEL
  unset -v MD_UUID
  unset -v MD_NAME
  unset -v MD_METADATA
  unset -v MD_DEVICES

  if [ "${MDADM_VERSION}" -lt 3001004 ] ; then
    eval "$(mdadm --detail ${1} --brief | sed -n -e 's#^ARRAY[[:space:]]\+[^[:space:]]*[[:space:]]*## ; s#\([^[:space:]]*=[^[:space:]]*\)[[:space:]]*#\1\n#g ; p' | sed -n -e '/^[[:space:]]*level/ { s#^[[:space:]]*level=#MD_LEVEL=# ; p ; } ; /^[[:space:]]*UUID=/ { s#^[[:space:]]*UUID=#MD_UUID=# ; p ; }')"
    eval "$(${CALL_ENV} mdadm --detail ${1} | sed -n -e 's#^[[:space:]]\+Version :[[:space:]]\+\([^.[:space:]]*\.[^.[:space:]]*\).*#MD_METADATA=\1#p')"
  else
    eval "$(mdadm --detail ${1} --export | grep -e '^[[:space:]]*MD_LEVEL=' -e '^[[:space:]]*MD_UUID=' -e '^[[:space:]]*MD_NAME=' -e '^[[:space:]]*MD_METADATA=')"
  fi

  if [ "${MDADM_VERSION}" -lt 3003002 ] ; then
    MD_DEVICES="$(${CALL_ENV} mdadm --detail ${TESTDEV} 2>/dev/null | sed -n -e '/^[[:space:]]*Number/,// { /^[[:space:]]*Number/d ; s#.*[[:space:]]\+\([^[:space:]]\+\)$#\1#p ; }' | xargs)"
  else
    MD_DEVICES="$(mdadm --detail ${1} --export | grep -e '^[[:space:]]*MD_DEVICE_.*_DEV=' | sed -e 's#^[[:space:]]*MD_DEVICE_.*_DEV=##' -e 's#[[:space:]]\+$##' | xargs)"
  fi

  case "${MD_METADATA}" in
   ('00.90') MD_METADATA='0.90' ;;
   ('01.00') MD_METADATA='1.0' ;;
   ('01.01') MD_METADATA='1.1' ;;
   ('01.02') MD_METADATA='1.2' ;;
  esac

  return 0
}

mdadm_wait () {
  sleep 1 ; # give raid some time to start sync
  if [ "${MDADM_VERSION}" -lt 2006007 ] ; then
    while ${CALL_ENV} mdadm --detail "${@}" | grep -q -F -e ' Rebuild Status'
     do
      sleep 5
    done
  else
    mdadm --wait "${@}"
  fi
  printf -- '\a'

  return 0
}

eval_lvm () {
  unset -v LVM2_LV_UUID
  unset -v LVM2_LV_NAME
  unset -v LVM2_VG_NAME
  unset -v LVM2_DEVICES

  if [ "${LVM_VERSION}" -lt 2002039 ] ; then
    eval "$(lvm lvs --noheadings -o LV_NAME,LV_UUID,VG_NAME ${1} 2>/dev/null | sed -n -e 's#^[[:space:]]*\([^[:space:]]*\)[[:space:]]*\([^[:space:]]*\)[[:space:]]*\([^[:space:]]*\)[[:space:]]*#LVM2_LV_NAME="\1"\nLVM2_LV_UUID="\2"\nLVM2_VG_NAME="\3"#p')"
  else
    eval "$(lvm lvs --rows --nameprefixes --noheadings -o LV_NAME,LV_UUID,VG_NAME ${1} 2>/dev/null | sed -n -e '/^[[:space:]]*LVM2_LV_UUID=/p ; /^[[:space:]]*LVM2_LV_NAME=/p ; /^[[:space:]]*LVM2_VG_NAME=/p')"
  fi
  LVM2_DEVICES="$(lvm lvs --noheadings -o DEVICES ${1} | sed -e 's#^[[:space:]]\+##' -e 's#(.\+)##' -e 's#[[:space:]]\+$##' | sort -u | xargs)"

  return 0
}

get_dmname () {
  local DMNUMBER

  unset -v DMNAME

  DMNAME="$(dmsetup info -C --noheadings -o name "${1}" 2>/dev/null)"
  [ -z "${DMNAME:-}" -a "${DEBVERSION}" -le 5 -a -h "${1}" ] && DMNAME="$(dmsetup info -C --noheadings -o name "$(readlink ${1})" 2>/dev/null)"
  if [ -z "${DMNAME:-}" -a -e "/sys/block/${1##*/}/dev" ]; then
    DMNUMBER="$(cat "/sys/block/${1##*/}/dev")"
    DMNAME="$(dmsetup info -C --noheadings -o name -j "${DMNUMBER%%:*}" -m "${DMNUMBER##*:}" 2>/dev/null)"
  fi

  return 0
}

create_lv () {
  # parameters:
  # 1 = LV Name
  # 2 = LV Size
  # 3 = VG

  if [ -z "${3:-}" ]; then
    printf -- 'Usage: create_lv <lv name> <lv size> <vg>\n' 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local LV
  local LVSIZE
  local VG

  LV="${1}"
  LVSIZE="${2}"
  VG="${3}"
  set --

  RC=0
  lvm lvs --noheadings -o LV_NAME "/dev/${VG}/${LV}" 1>/dev/null 2>&1 || RC="${?}"
  if [ ${RC} -eq 0 ] ; then
    # lv already exists
    :
  else
    printf -- '...creating logical volume /dev/%s/%s\n' "${VG}" "${LV}"
    lvm lvcreate -n "${LV}" -L "${LVSIZE}" "${VG}" || { return 1 2>/dev/null || exit 1 ; }
  fi
#
  lvm lvchange -a y "/dev/${VG}/${LV}" || { return 1 2>/dev/null || exit 1 ; }
#
  eval_blkid "/dev/${VG}/${LV}"
  if [ -z "${TYPE:-}" ]; then
    mkfs.ext4 "/dev/${VG}/${LV}"
    blkid "/dev/${VG}/${LV}" >/dev/null ; sync ; # make sure that super block is created and written
  fi

  return 0
}


chmod_ssh_dir () {
  # parameters:
  # 1 = SSH Dir
  # 2 = User Name

  if [ -z "${2:-}" ]; then
    printf -- 'Usage: chmod_ssh_dir <ssh dir> <user name>\n' 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local USERSSHDIR
  local USERNAME
  local USERGROUP

  USERSSHDIR="${1}"
  USERNAME="${2}"
  USERGROUP="${2}"
  set --

  find -P "${USERSSHDIR}" -type d \( -name 'lost+found' \) -prune -o \( -type f -o -type d \) ! \( -user "${USERNAME}" -a -group "${USERGROUP}" \) -exec chown -h "${USERNAME}:${USERGROUP}" '{}' \;
  find -P "${USERSSHDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
  find -P "${USERSSHDIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,go= -exec chmod u=rwx-s,go=-s '{}' \;

  return 0
}


chmod_home_dir () {
  ###
  ### fixes permissions in home directory
  ###
  ## Parameters:
  ## 1 = Home Dir
  ## 2 = User Name
  ## 3 = Group Name
  ## 4 = File Perm
  ## 5 = Dir Perm
  ## 6 = Script Perm

  if [ -z "${6:-}" ]; then
    printf -- 'Usage: chmod_home_dir <dir> <user name> <user group> <file perm> <dir perm> <script perm>\n' 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local DIR
  local USERNAME
  local USERGROUP
  local FILEPERM
  local DIRPERM
  local SCRIPTPERM

  local BINDIR
  local BINDIR1
  local BINDIR2
  local SSHDIR
  local SVNDIR
  local GITSHDIR

  local SVNUSERNAME
  local SVNSIZE
  local SVNREPOPARENTDIR
  local SVNREPOBASEDIR
  local SVNREPODIR

  local PRUNESVN
  local PRUNEJAIL
  local FINDCMD

  DIR="${1}"
  USERNAME="${2}"
  USERGROUP="${3:-${USERNAME}}"
  FILEPERM="${4}"
  DIRPERM="${5}"
  SCRIPTPERM="${6}"
  set --

  BINDIR1="${DIR}/bin"
  BINDIR2="${DIR}/work/bin"
  SSHDIR="${DIR}/.ssh"
  SVNDIR="${DIR}/.subversion"
  GITSHDIR="${DIR}/git-shell-commands"

  unset -v SVNREPOBASEDIR
  unset -v PRUNESVN
  if [ -s '/etc/custom/subversion' ]; then
    . /etc/custom/subversion
    [ -n "${SVNREPOBASEDIR:-}" -a ! -d "${SVNREPOBASEDIR}" ] && unset -v SVNREPOBASEDIR
    [ -z "${SVNREPOBASEDIR:-}" ] || PRUNESVN="-o -path \"${SVNREPOBASEDIR}\""
  fi

  PRUNEJAIL="-o -path \"${DIR}/bin\" -o -path \"${DIR}/boot\" -o -path \"${DIR}/dev\" -o -path \"${DIR}/etc\" -o -path \"${DIR}/home\" -o -path \"${DIR}/lib*\" -o -path \"${DIR}/media\" -o -path \"${DIR}/mnt\" -o -path \"${DIR}/opt\" -o -path \"${DIR}/proc\" -o -path \"${DIR}/proc/*\" -o -path \"${DIR}/root\" -o -path \"${DIR}/run\" -o -path \"${DIR}/sbin\" -o -path \"${DIR}/srv\" -o -path \"${DIR}/sys\" -o -path \"${DIR}/tmp\" -o -path \"${DIR}/usr\" -o -path \"${DIR}/var\""

  ## Owner/Group and Permissions
  printf -- '%s: fixing owner/group and permissions in %s... (may take a while)\n' "${0}" "${DIR}"
  ## --> SSH  (do first, so if cancelled later then this is already secured)
  if [ -d "${SSHDIR}" ]; then
    printf -- '...ssh dir\n'
    chmod_ssh_dir "${SSHDIR}" "${USERNAME}"
  fi
  ## --> bin dir
  for BINDIR in ${BINDIR1} ${BINDIR2}
   do
    if [ -d "${BINDIR}" ]; then
      printf -- '...bin dir\n'
      find -P "${BINDIR}" -type d \( -name 'lost+found' \) -prune -o \( -type f -o -type d \) ! \( -user "${USERNAME}" -a -group "${USERGROUP}" \) -exec chown -h "${USERNAME}:${USERGROUP}" '{}' \;
      find -P "${BINDIR}" -type d \( -name 'lost+found' \) -prune -o -type f -name "*.sh" ! -perm "${SCRIPTPERM}" -exec chmod "${SCRIPTPERM}" '{}' \;
      find -P "${BINDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -name "*.sh" ! -perm "${FILEPERM}" -exec chmod "${FILEPERM}" '{}' \;
      find -P "${BINDIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm "${DIRPERM}" -exec chmod "${DIRPERM}" '{}' \;
    fi
  done
  ## --> mysql account files
  if [ -f "${DIR}/.my.cnf" -o -f "${DIR}/.mysql_accounts" ]; then
    printf -- '...mysql account files\n'
    [ ! -f "${DIR}/.my.cnf" ] || chmod u=rw,go= "${DIR}/.my.cnf"
    [ ! -f "${DIR}/.mysql_accounts" ] || chmod u=rw,go= "${DIR}/.mysql_accounts"
  fi
  ## --> mail account files
  if [ -f "${DIR}/.mail_accounts" ]; then
    printf -- '...mail account files\n'
    [ ! -f "${DIR}/.mail_accounts" ] || chmod u=rw,go= "${DIR}/.mail_accounts"
  fi
  ## --> www account files
  if [ -f "${DIR}/.www_accounts" ]; then
    printf -- '...www account files\n'
    [ ! -f "${DIR}/.www_accounts" ] || chmod u=rw,go= "${DIR}/.www_accounts"
  fi
  ## --> .subversion dir
  if [ -d "${SVNDIR}" ]; then
    printf -- '...subversion dir\n'
    find -P "${SVNDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rw,og= -exec chmod u=rw,og= '{}' \;
    find -P "${SVNDIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,og= -exec chmod u=rwx-s,og=-s '{}' \;
  fi
  ## --> git-shell-commands dir
  if [ -d "${GITSHDIR}" ]; then
    printf -- '...git-shell-commands dir\n'
    find -P "${GITSHDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rwx,og= -exec chmod u=rwx,og= '{}' \;
  fi

  ## --> Owner/Group
  printf -- '...owner/group\n'
  FINDCMD="find -P \"${DIR}\" -mindepth 1 -type d \( -name 'lost+found' -o -path \"${DIR}/.local\" -o -path \"${DIR}/setup_files\" -o -path \"${DIR}/mail\" -o -path \"${DIR}/www\" -o -path \"${SSHDIR}\" -o -path \"${BINDIR1}\" -o -path \"${BINDIR2}\" -o -path \"${SVNDIR}\" -o -path \"${GITSHDIR}\" ${PRUNEJAIL} ${PRUNESVN:-} \) -prune -o \( -type f -o -type d \) ! \( -user \"${USERNAME}\" -a -group \"${USERGROUP}\" \) -exec chown -h \"${USERNAME}:${USERGROUP}\" '{}' \;"
  eval ${FINDCMD}
  ## --> Directories
  printf -- '...directory permissions\n'
  FINDCMD="find -P \"${DIR}\" -mindepth 1 -type d \( -name 'lost+found' -o -path \"${DIR}/.local\" -o -path \"${DIR}/setup_files\" -o -path \"${DIR}/mail\" -o -path \"${DIR}/www\" -o -path \"${SSHDIR}\" -o -path \"${BINDIR1}\" -o -path \"${BINDIR2}\" -o -path \"${SVNDIR}\" -o -path \"${GITSHDIR}\" ${PRUNEJAIL} ${PRUNESVN:-} \) -prune -o -type d ! -perm \"${DIRPERM}\" -exec chmod \"${DIRPERM}\" '{}' \;"
  eval ${FINDCMD}
  ## --> Files (do at the end, so if cancelled the previous stuff is already secured)
  printf -- '...file permissions\n'
  FINDCMD="find -P \"${DIR}\" -mindepth 1 -type d \( -name 'lost+found' -o -path \"${DIR}/.local\" -o -path \"${DIR}/setup_files\" -o -path \"${DIR}/mail\" -o -path \"${DIR}/www\" -o -path \"${SSHDIR}\" -o -path \"${BINDIR1}\" -o -path \"${BINDIR2}\" -o -path \"${SVNDIR}\" -o -path \"${GITSHDIR}\" ${PRUNEJAIL} ${PRUNESVN:-} \) -prune -o -type f ! \( -perm \"${FILEPERM}\" -o -path \"${DIR}/.my.cnf\" -o -path \"${DIR}/.mysql_accounts\" -o -path \"${DIR}/.mail_accounts\" -o -path \"${DIR}/.www_accounts\" \) -exec chmod \"${FILEPERM}\" '{}' \;"
  eval ${FINDCMD}

  return 0
}

chmod_home_dir_user_group_only () {
  ###
  ### fixes permissions in home directory, so that user and group have access to files
  ###
  ## Parameters:
  ## 1 = User Name
  ## 2 = Group Name
  ## 3 = Dir Patterns

  if [ -z "${1:-}" ]; then
    printf -- 'Usage: chmod_home_dir_user_group_only <user name> [<group name> [<dir pattern(s)>]]\n' 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local USERNAME
  local USERGROUP
  local DIRPATTERN
  local DIR

  USERNAME="${1}"
  USERGROUP="${2:-${USERNAME}}"
  DIRPATTERN="${3:-}"
  if [ -z "${DIRPATTERN:-}" ]; then
    [ "${USERNAME}" != 'root' ] || DIRPATTERN='/root'
    DIRPATTERN="${DIRPATTERN:+${DIRPATTERN} }/home/${USERNAME}"
  fi
  set --

  for DIR in ${DIRPATTERN}
   do
    [ -d "${DIR}" ] || continue
    chmod_home_dir "${DIR}" "${USERNAME}" "${USERGROUP}" 'ug=rw,o=' 'ug=rwx-s,o=-s' 'ug=rwx,o='
  done

  return 0
}

chmod_home_dir_user_only () {
  ###
  ### fixes permissions in home directory, so that only user has access to files
  ###
  ## Parameters:
  ## 1 = User Name
  ## 2 = Dir Patterns

  if [ -z "${1:-}" ]; then
    printf -- 'Usage: chmod_home_dir_user_only <user name> [<dir pattern(s)>]\n' 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local USERNAME
  local USERGROUP
  local DIRPATTERN
  local DIR

  USERNAME="${1}"
  USERGROUP="${1}"
  DIRPATTERN="${2:-}"
  if [ -z "${DIRPATTERN:-}" ]; then
    [ "${USERNAME}" != 'root' ] || DIRPATTERN='/root'
    DIRPATTERN="${DIRPATTERN:+${DIRPATTERN} }/home/${USERNAME}"
  fi
  set --

  for DIR in ${DIRPATTERN}
   do
    [ -d "${DIR}" ] || continue
    chmod_home_dir "${DIR}" "${USERNAME}" "${USERGROUP}" 'u=rw,go=' 'u=rwx-s,go=-s' 'u=rwx,go='
  done

  return 0
}


chmod_etc_custom_dir () {
  # parameters:
  # 1 = Dir Pattern

  if [ -z "${1:-}" ]; then
    printf -- 'Usage: chmod_etc_custom_dir <dir pattern(s)>\n' 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local DIRPATTERN
  local DIR
  local PRIVATEDIR

  DIRPATTERN="${1}"
  set --

  for DIR in ${DIRPATTERN}
   do
    [ -d "${DIR}" ] || continue
    printf -- '%s: fixing owner/group and permissions in %s... (may take a while)\n' "${0}" "${DIR}"
    PRIVATEDIR="${DIR}/private"

    ## --> private dir
    if [ -d "${PRIVATEDIR}" ]; then
      find -P "${PRIVATEDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=r,go= -exec chmod u=r,go= '{}' \;
    fi
    ## --> custom key files
    find -P "${DIR}" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/*.key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;

    ## --> Directories
    printf -- '...directory permissions\n'
    find -P "${DIR}" -type d \( -name 'lost+found' -o -path "${PRIVATEDIR}" \) -prune -o -type d ! -perm u=rwx,go=rx -exec chmod u=rwx-s,go=rx-s '{}' \;
    ## --> Files (do at the end, so if cancelled the previous stuff is already secured)
    printf -- '...file permissions\n'
    find -P "${DIR}" -type d \( -name 'lost+found' -o -path "${PRIVATEDIR}" \) -prune -o -type f ! \( -path "${DIR}/*.key" \) ! -perm u=rw,go=r -exec chmod u=rw,go=r '{}' \;
  done

  return 0
}


chmod_etc_dir () {
  # parameters:
  # 1 = Dir Pattern

  if [ -z "${1:-}" ]; then
    printf -- 'Usage: chmod_etc_dir <dir pattern(s)>\n' 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local DIRPATTERN
  local DIR
  local SSLPRIVDIR
  local POSTFIXDIR
  local LETSENCACCDIR

  DIRPATTERN="${1}"
  set --

  for DIR in ${DIRPATTERN}
   do
    [ -d "${DIR}" ] || continue
    printf -- '%s: fixing owner/group and permissions in %s... (may take a while)\n' "${0}" "${DIR}"
    SSLPRIVDIR="${DIR}/ssl/private"
    POSTFIXDIR="${DIR}/postfix"
    LETSENCACCDIR="${DIR}/letsencrypt/accounts"

    ## --> letsencrypt account dir
    if [ -d "${LETSENCACCDIR}" ]; then
      find -P "${LETSENCACCDIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,go= -exec chmod u=rwx-s,go=-s '{}' \;
      find -P "${LETSENCACCDIR}" -type d \( -name 'lost+found' \) -prune -o -type f -name "private_key.json" ! -perm u=r,go= -exec chmod u=r,go= '{}' \;
    fi
    ## --> custom dir
    if [ -d "${DIR}/custom" ]; then
      chmod_etc_custom_dir "${DIR}/custom"
    fi
    ## --> ssl private dir
    if [ -d "${SSLPRIVDIR}" ]; then
      find -P "${SSLPRIVDIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,go= -exec chmod u=rwx-s,go=-s '{}' \;
      find -P "${SSLPRIVDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    fi
    ## --> cron.hourly dir (all files)
    if [ -d "${DIR}/cron.hourly" ]; then
      find -P "${DIR}/cron.hourly" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rwx,go=rx -exec chmod u=rwx,go=rx '{}' \;
    fi
    ## --> init.d dir (all files)
    if [ -d "${DIR}/init.d" ]; then
      find -P "${DIR}/init.d" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rwx,go=rx -exec chmod u=rwx,go=rx '{}' \;
    fi
    ## --> postfix dir (all files)
    if [ -d "${POSTFIXDIR}" ]; then
      find -P "${POSTFIXDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rw,g=r,o= -exec chmod u=rw,g=r,o= '{}' \;
    fi
    ## --> bind key files
    if [ -d "${DIR}/bind" ]; then
      find -P "${DIR}/bind" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/bind/*.key" ! -perm u=rw,g=r,o= -exec chmod u=rw,g=r,o= '{}' \;
      find -P "${DIR}/bind" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/bind/*.key" ! \( -user "root" -a -group "bind" \) -exec chown -h "root:bind" '{}' \; || true
    fi
    ## --> dropbear key files
    if [ -d "${DIR}/dropbear" ]; then
      find -P "${DIR}/dropbear" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/dropbear/dropbear_*_host_key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    fi
    ## --> dropbear initramfs key files
    if [ -d "${DIR}/dropbear-initramfs" ]; then
      find -P "${DIR}/dropbear-initramfs" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/dropbear-initramfs/dropbear_*_host_key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    fi
    ## --> initramfs dropbear key files
    if [ -d "${DIR}/initramfs-tools/etc/dropbear" ]; then
      find -P "${DIR}/initramfs-tools/etc/dropbear" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/initramfs-tools/etc/dropbear/dropbear_*_host_key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    fi
    ## --> ssh key files
    if [ -d "${DIR}/ssh" ]; then
      find -P "${DIR}/ssh" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/ssh/ssh_host_*_key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    fi

    ## --> Directories
    printf -- '...directory permissions\n'
    find -P "${DIR}" -type d \( -name 'lost+found' -o -path "${LETSENCACCDIR}" -o -path "${DIR}/custom" -o -path "${SSLPRIVDIR}" \) -prune -o -type d ! -perm u=rwx,go=rx -exec chmod u=rwx-s,go=rx-s '{}' \;
    ## --> Files (do at the end, so if cancelled the previous stuff is already secured)
    printf -- '...file permissions\n'
    find -P "${DIR}" -type d \( -name 'lost+found' -o -path "${LETSENCACCDIR}" -o -path "${DIR}/custom" -o -path "${SSLPRIVDIR}" -o -path "${DIR}/cron.hourly" -o -path "${DIR}/init.d" -o -path "${POSTFIXDIR}" \) -prune -o -type f ! \( -path "${DIR}/bind/*.key" -o -path "${DIR}/dropbear/dropbear_*_host_key" -o -path "${DIR}/dropbear-initramfs/dropbear_*_host_key" -o -path "${DIR}/initramfs-tools/etc/dropbear/dropbear_*_host_key" -o -path "${DIR}/ssh/ssh_host_*_key" \) ! -perm u=rw,go=r -exec chmod u=rw,go=r '{}' \;
  done

  return 0
}


chmod_lib_dir () {
  # parameters:
  # 1 = Dir Pattern

  if [ -z "${1:-}" ]; then
    printf -- 'Usage: chmod_lib_dir <dir pattern(s)>\n' 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local DIRPATTERN
  local DIR
  local SYSTEMDSYSTEMDIR

  DIRPATTERN="${1}"
  set --

  for DIR in ${DIRPATTERN}
   do
    [ -d "${DIR}" ] || continue
    printf -- '%s: fixing owner/group and permissions in %s... (may take a while)\n' "${0}" "${DIR}"
    SYSTEMDSYSTEMDIR="${DIR}/systemd/system"

    ## --> systemd .service files
    if [ -d "${SYSTEMDSYSTEMDIR}" ]; then
      printf -- '...systemd .service files\n'
      find -P "${SYSTEMDSYSTEMDIR}" -maxdepth 1 -type f -name "*.service" ! -perm u=rw,go=r -exec chmod u=rw,go=r '{}' \;
    fi
  done

  return 0
}


chmod_usr_dir () {
  # parameters:
  # 1 = Dir Pattern

  if [ -z "${1:-}" ]; then
    printf -- 'Usage: chmod_usr_dir <dir pattern(s)>\n' 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local DIRPATTERN
  local DIR
  local BINDIR
  local SBINDIR

  DIRPATTERN="${1}"
  set --

  for DIR in ${DIRPATTERN}
   do
    [ -d "${DIR}" ] || continue
    printf -- '%s: fixing owner/group and permissions in %s... (may take a while)\n' "${0}" "${DIR}"
    BINDIR="${DIR}/local/bin"
    SBINDIR="${DIR}/local/sbin"

    ## --> bin .sh files
    if [ -d "${BINDIR}" ]; then
      printf -- '...scripts\n'
      find -P "${BINDIR}" -maxdepth 1 -type f -name "*.sh" ! -perm u=rwx,go=rx -exec chmod u=rwx,go=rx '{}' \;
    fi
    ## --> sbin .sh files
    if [ -d "${SBINDIR}" ]; then
      printf -- '...scripts\n'
      find -P "${SBINDIR}" -maxdepth 1 -type f -name "*.sh" ! -perm u=rwx,go=rx -exec chmod u=rwx,go=rx '{}' \;
    fi

    ## --> Directories
    printf -- '...directory permissions\n'
    find -P "${DIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,go=rx -exec chmod u=rwx-s,go=rx-s '{}' \;
    ## --> Files (do at the end, so if cancelled the previous stuff is already secured)
    printf -- '...file permissions\n'
    find -P "${DIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! \( -perm u=rw,go=r -o -path "${BINDIR}/*.sh" -o -path "${SBINDIR}/*.sh" \) -exec chmod u=rw,go=r '{}' \;
  done

  return 0
}
