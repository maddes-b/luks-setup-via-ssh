#!/bin/false

echo 'Setting functions...' 1>&2

[ ! -f '/etc/debian_version' ] && { echo 'Not a Debian system'; return 1 2>/dev/null || exit 1; }

CUSTOMSETUPFILES='/home/root/setup_files'
#
unset APTVERSION
[ -s /etc/custom/debian ] && . /etc/custom/debian
DEBSIDRELEASE='12'
#
STANDARDIFS="$(printf ' \t\nX')" ; STANDARDIFS="${STANDARDIFS%X}"
NEWLINEIFS="$(printf '\nX')" ; NEWLINEIFS="${NEWLINEIFS%X}"

### Distribution
debversion_to_release () {
  case "${DEBVERSION}" in
   ('3') DEBRELEASE='sarge' ;;
   ('4') DEBRELEASE='etch' ;;
   ('5') DEBRELEASE='lenny' ;;
   ('6') DEBRELEASE='squeeze' ;;
   ('7') DEBRELEASE='wheezy' ;;
   ('8') DEBRELEASE='jessie' ;;
   ('9') DEBRELEASE='stretch' ;;
   (${DEBSIDRELEASE}) DEBRELEASE='sid' ;;
  esac

  return 0
}

debrelease_to_version () {
  case "${DEBRELEASE}" in
   ('sarge')   DEBVERSION='3' ;;
   ('etch')    DEBVERSION='4' ;;
   ('lenny')   DEBVERSION='5' ;;
   ('squeeze') DEBVERSION='6' ;;
   ('wheezy')  DEBVERSION='7' ;;
   ('jessie')  DEBVERSION='8' ;;
   ('stretch') DEBVERSION='9' ;;
   ('sid')     DEBVERSION="${DEBSIDRELEASE}" ;;
  esac

  return 0
}

unset DIST_NAME
unset DIST_NAME_LOWER
unset DIST_VERSION
unset DIST_MAJOR
unset DIST_MINOR
unset DIST_MICRO
unset DEBRELEASE
RC=0
which lsb_release >/dev/null || RC="${?}"
if [ "${RC}" -eq 0 ] ; then
  DIST_NAME="$(lsb_release -i -s)"
  grep -q -F -e 'OSMC' /etc/issue.net && DIST_NAME='OSMC'
  DIST_VERSION="$(lsb_release -r -s)"
  DEBRELEASE="$(lsb_release -c -s)"
  [ "${DIST_NAME}" = 'Debian' -a "${DIST_VERSION}" = 'testing' ] && { debrelease_to_version; DIST_VERSION="${DEBVERSION}.0" ; }
  [ "${DIST_NAME}" = 'Debian' -a "${DEBRELEASE}" = 'sid' ] && DIST_VERSION="${DEBSIDRELEASE}.0"
else
  DIST_NAME='Debian'
  grep -q -F -e 'Ubuntu' /etc/issue.net && DIST_NAME='Ubuntu'
  grep -q -F -e 'OSMC' /etc/issue.net && DIST_NAME='OSMC'
  grep -q -F -e 'Raspbian' /etc/issue.net && DIST_NAME='Raspbian'
  case "${DIST_NAME}" in
    ('Ubuntu') DIST_VERSION="$(sed -n -e 's#[^0-9]*\([0-9.]*\)#\1#p' /etc/issue.net)" ;;
    (*) DIST_VERSION="$(cat /etc/debian_version)"
        DIST_VERSION="$(echo "${DIST_VERSION}" | sed 's#.*/\(sid\)#\1#')"
        [ "${DIST_NAME}" = 'Debian' -a "${DIST_VERSION}" = 'sid' ] && DIST_VERSION="${DEBSIDRELEASE}.0"
        ;;
  esac
fi
[ -n "${DIST_NAME}" ] && DIST_NAME_LOWER="$(echo "${DIST_NAME}" | sed 's#.*#\L&#')"

DIST_MAJOR="$(echo "${DIST_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
DIST_MINOR="$(echo "${DIST_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
DIST_VERSION="$(( ${DIST_MAJOR:-0} * 1000000 + ${DIST_MINOR:-0} * 1000 + ${DIST_MICRO:-0} ))"

DEBIAN=0
UBUNTU=0
OSMC=0
RASPBIAN=0
SPARKY=0
case "${DIST_NAME}" in
 ('Ubuntu')
  UBUNTU=1
  case "${DIST_VERSION}" in
   (15004000)
    DEBVERSION='8'
    DEBVERSION='7'
    [ -z "${DEBRELEASE}" ] && DEBRELEASE='vivid'
    ;;
   (15010000)
    DEBVERSION='8'
    [ -z "${DEBRELEASE}" ] && DEBRELEASE='wily'
    ;;
   (16004000)
    DEBVERSION='8'
    [ -z "${DEBRELEASE}" ] && DEBRELEASE='xenial'
    ;;
  esac
  ;;
 ('OSMC')
  OSMC=1
  DEBVERSION="${DIST_MAJOR}"
  [ -z "${DEBRELEASE}" ] && debversion_to_release
  ;;
 ('Raspbian')
  RASPBIAN=1
  DEBVERSION="${DIST_MAJOR}"
  [ -z "${DEBRELEASE}" ] && debversion_to_release
  ;;
 ('Sparky')
  SPARKY=1
  DEBIAN=1
  DEBVERSION="$(cat /etc/debian_version)"
  DEBVERSION="$(echo "${DEBVERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  DEBRELEASE='testing'
  APTVERSION='sparky'
  ;;
 (*)
  DEBIAN=1
  DEBVERSION="${DIST_MAJOR}"
  [ -z "${DEBRELEASE}" ] && debversion_to_release
  ;;
esac
[ -z "${APTVERSION}" ] && APTVERSION="${DEBVERSION}"

[ -z "${HOSTNAME}" ] && HOSTNAME="$(hostname)"
[ -z "${HOSTNAME}" -a -f /etc/hostname ] && HOSTNAME="$(cat /etc/hostname)"

MAXDEBVERSION="$(( ${DEBVERSION:-0} + 5 ))"
[ "${MAXDEBVERSION}" -lt 10 ] && MAXDEBVERSION='10'
DEBVERSIONS="$(seq 1 ${MAXDEBVERSION})"

unset CPNOCLOBBER
[ "${DEBVERSION}" -ge 6 ] && CPNOCLOBBER='-n'

### Linux Kernel
# Debian 4.x: 2.6.18-4-amd64
# Debian 5.x: 2.6.26-1-amd64
# Debian 6.x: 2.6.32-5-amd64
# Debian 7.x: 3.2.0-4-amd64
# Debian 8.x: 3.16.0-4-amd64
# Debian 9.x: 4.9.0-3-amd64
unset LINUX_VERSION
unset LINUX_MAJOR
unset LINUX_MINOR
unset LINUX_MICRO
unset LINUX_ARCH
which uname >/dev/null && {
  LINUX_VERSION="$(uname -r)"
  LINUX_MAJOR="$(echo "${LINUX_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  LINUX_MINOR="$(echo "${LINUX_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  LINUX_MICRO="$(echo "${LINUX_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  LINUX_ARCH="$(echo "${LINUX_VERSION}" | grep -o -e '[^-]*$')"
  LINUX_VERSION="$(( ${LINUX_MAJOR:-0} * 1000000 + ${LINUX_MINOR:-0} * 1000 + ${LINUX_MICRO:-0} ))"
}

### BLKID
# Debian 4.x: 1.0.0
# Debian 5.x: 1.0.0
# Debian 6.x: 2.17.2
# Debian 7.x: 2.20.1 (-o export)
# Debian 8.x: 2.25.2
# Debian 9.x: 2.29.2
unset BLKID_VERSION
unset BLKID_MAJOR
unset BLKID_MINOR
unset BLKID_MICRO
which blkid >/dev/null && {
  BLKID_VERSION="$(blkid -v | sed -n -e 's#[^0-9]*\([0-9.]*\).*#\1#p')"
  BLKID_MAJOR="$(echo "${BLKID_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  BLKID_MINOR="$(echo "${BLKID_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  BLKID_MICRO="$(echo "${BLKID_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  BLKID_VERSION="$(( ${BLKID_MAJOR:-0} * 1000000 + ${BLKID_MINOR:-0} * 1000 + ${BLKID_MICRO:-0} ))"
}

### MDADM
# Debian 4.x: 2.5.6
# Debian 5.x: 2.6.7.2 (--wait)
# Debian 6.x: 3.1.4 (--export)
# Debian 7.x: 3.2.5
# Debian 8.x: 3.3.2 (--export: MD_DEVICE_*_DEV)
# Debian 9.x: 3.4
unset MDADM_VERSION
unset MDADM_MAJOR
unset MDADM_MINOR
unset MDADM_MICRO
which mdadm >/dev/null && {
  MDADM_VERSION="$(mdadm --version 2>&1 | sed -n -e 's#mdadm - v\(.*\) - .*#\1#p')"
  MDADM_MAJOR="$(echo "${MDADM_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  MDADM_MINOR="$(echo "${MDADM_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  MDADM_MICRO="$(echo "${MDADM_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  MDADM_VERSION="$(( ${MDADM_MAJOR:-0} * 1000000 + ${MDADM_MINOR:-0} * 1000 + ${MDADM_MICRO:-0} ))"
}

### LVM
# Debian 4.x: 2.02.07 / 1.02.08 / 4.7.0
# Debian 5.x: 2.02.39 / 1.02.27 / 4.13.0 (--rows --nameprefixes)
# Debian 6.x: 2.02.66 / 1.02.48 / 4.15.0
# Debian 7.x: 2.02.95 / 1.02.74 / 4.22.0
# Debian 8.x: 2.02.111 / 1.02.90 / 4.27.0
# Debian 9.x: 2.02.168 / 1.02.137 / 4.35.0
unset LVM_VERSION
unset LVM_MAJOR
unset LVM_MINOR
unset LVM_MICRO
which lvm >/dev/null && {
  LVM_VERSION="$(lvm version | sed -n -e 's#[[:space:]]*LVM version:[[:space:]]*\([0-9.]*\).*#\1#p')"
  LVM_MAJOR="$(echo "${LVM_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  LVM_MINOR="$(echo "${LVM_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  LVM_MICRO="$(echo "${LVM_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  LVM_VERSION="$(( ${LVM_MAJOR:-0} * 1000000 + ${LVM_MINOR:-0} * 1000 + ${LVM_MICRO:-0} ))"
}

### LUKS/cryptsetup
# Debian 4.x: 1.0.5
# Debian 5.x: 1.0.6
# Debian 6.x: 1.1.3
# Debian 7.x: 1.4.3
# Debian 8.x: 1.6.6
# Debian 9.x: 1.7.3
unset LUKS_VERSION
unset LUKS_MAJOR
unset LUKS_MINOR
unset LUKS_MICRO
which cryptsetup >/dev/null && {
  LUKS_VERSION="$(cryptsetup --version | sed -n -e 's#[^0-9]*\([0-9.]*\).*#\1#p')"
  LUKS_MAJOR="$(echo "${LUKS_VERSION}" | sed -n -e 's#\([0-9]*\).*#\1#p')"
  LUKS_MINOR="$(echo "${LUKS_VERSION}" | sed -n -e 's#[0-9]*\.\([0-9]*\).*#\1#p')"
  LUKS_MICRO="$(echo "${LUKS_VERSION}" | sed -n -e 's#[0-9]*\.[0-9]*\.\([0-9]*\).*#\1#p')"
  LUKS_VERSION="$(( ${LUKS_MAJOR:-0} * 1000000 + ${LUKS_MINOR:-0} * 1000 + ${LUKS_MICRO:-0} ))"
}

### Command to restart service
unset RESTARTCMD1
unset RESTARTCMD2
RESTARTCMD1="$(which systemctl)" && RESTARTCMD1="${RESTARTCMD1} restart"
#
[ -z "${RESTARTCMD1}" ] && {
  RESTARTCMD1="$(which service)" && RESTARTCMD2='restart'
}
#
[ -z "${RESTARTCMD1}" ] && {
  RESTARTCMD1="$(which invoke-rc.d)" && RESTARTCMD2='restart'
}


eval_blkid () {
  unset UUID
  unset UUID_SUB
  unset LABEL
  unset TYPE

  if [ "${BLKID_VERSION}" -lt 2020001 ] ; then
    eval "$(blkid -o full ${1} 2>/dev/null | sed -n -e 's#^.*: ## ; s#\([^[:space:]]*="[^"]*"\)[[:space:]]*#\1\n#g ; p' | grep -e '^[[:space:]]*UUID=' -e '^[[:space:]]*UUID_SUB=' -e '^[[:space:]]*LABEL=' -e '^[[:space:]]*TYPE=')"
  else
    eval "$(blkid -o export ${1} 2>/dev/null | grep -e '^[[:space:]]*UUID=' -e '^[[:space:]]*UUID_SUB=' -e '^[[:space:]]*LABEL=' -e '^[[:space:]]*TYPE=')"
  fi

  return 0
}

eval_mdadm () {
  unset MD_LEVEL
  unset MD_UUID
  unset MD_NAME
  unset MD_METADATA
  unset MD_DEVICES

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
    while ${CALL_ENV} mdadm --detail ${1} | grep -q -F -e ' Rebuild Status'
     do
      sleep 5
    done
  else
    mdadm --wait "${@}"
  fi
  printf '\a'

  return 0
}

eval_lvm () {
  unset LVM2_LV_UUID
  unset LVM2_LV_NAME
  unset LVM2_VG_NAME
  unset LVM2_DEVICES

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

  unset DMNAME

  DMNAME="$(dmsetup info -C --noheadings -o name "${1}" 2>/dev/null)"
  [ -z "${DMNAME}" -a "${DEBVERSION}" -le 5 -a -h "${1}" ] && DMNAME="$(dmsetup info -C --noheadings -o name "$(readlink ${1})" 2>/dev/null)"
  [ -z "${DMNAME}" -a -e "/sys/block/${1##*/}/dev" ] && {
    DMNUMBER="$(cat "/sys/block/${1##*/}/dev")"
    DMNAME="$(dmsetup info -C --noheadings -o name -j "${DMNUMBER%%:*}" -m "${DMNUMBER##*:}" 2>/dev/null)"
  }

  return 0
}

create_lv () {
  # parameters:
  # 1 = LV Name
  # 2 = LV Size
  # 3 = VG

  [ -z "${3}" ] && {
    echo "Usage: create_lv <lv name> <lv size> <vg>" 1>&2
    return 1 2>/dev/null || exit 1
  }

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
    echo "...creating logical volume /dev/${VG}/${LV}"
    lvm lvcreate -n "${LV}" -L "${LVSIZE}" "${VG}" || { return 1 2>/dev/null || exit 1 ; }
  fi
#
  lvm lvchange -a y "/dev/${VG}/${LV}" || { return 1 2>/dev/null || exit 1 ; }
#
  eval_blkid "/dev/${VG}/${LV}"
  [ -z "${TYPE}" ] && {
    mkfs.ext4 "/dev/${VG}/${LV}"
    blkid "/dev/${VG}/${LV}" >/dev/null ; sync ; # make sure that super block is created and written
  }

  return 0
}


chmod_ssh_dir () {
  # parameters:
  # 1 = SSH Dir
  # 2 = User Name

  [ -z "${2}" ] && {
    echo "Usage: chmod_ssh_dir <ssh dir> <user name>" 1>&2
    return 1 2>/dev/null || exit 1
  }

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

  [ -z "${6}" ] && {
    echo "Usage: chmod_home_dir <dir> <user name> <user group> <file perm> <dir perm> <script perm>" 1>&2
    return 1 2>/dev/null || exit 1
  }

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
  USERGROUP="${3}"
  [ -z "${USERGROUP}" ] && USERGROUP="${USERNAME}"
  FILEPERM="${4}"
  DIRPERM="${5}"
  SCRIPTPERM="${6}"
  set --

  BINDIR1="${DIR}/bin"
  BINDIR2="${DIR}/work/bin"
  SSHDIR="${DIR}/.ssh"
  SVNDIR="${DIR}/.subversion"

  unset SVNREPOBASEDIR
  unset PRUNESVN
  [ -s /etc/custom/subversion ] && {
    . /etc/custom/subversion
    [ -n "${SVNREPOBASEDIR}" -a ! -d "${SVNREPOBASEDIR}" ] && unset SVNREPOBASEDIR
    [ -n "${SVNREPOBASEDIR}" ] && PRUNESVN="-o -path \"${SVNREPOBASEDIR}\""
  }

  PRUNEJAIL="-o -path \"${DIR}/bin\" -o -path \"${DIR}/boot\" -o -path \"${DIR}/dev\" -o -path \"${DIR}/etc\" -o -path \"${DIR}/home\" -o -path \"${DIR}/lib*\" -o -path \"${DIR}/media\" -o -path \"${DIR}/mnt\" -o -path \"${DIR}/opt\" -o -path \"${DIR}/proc\" -o -path \"${DIR}/proc/*\" -o -path \"${DIR}/root\" -o -path \"${DIR}/run\" -o -path \"${DIR}/sbin\" -o -path \"${DIR}/srv\" -o -path \"${DIR}/sys\" -o -path \"${DIR}/tmp\" -o -path \"${DIR}/usr\" -o -path \"${DIR}/var\""

  ## Owner/Group and Permissions
  echo "${0}: fixing owner/group and permissions in ${DIR}... (may take a while)"
  ## --> SSH  (do first, so if cancelled later then this is already secured)
  [ -d "${SSHDIR}" ] && {
    echo "...ssh dir"
    chmod_ssh_dir "${SSHDIR}" "${USERNAME}"
  }
  ## --> bin dir
  for BINDIR in ${BINDIR1} ${BINDIR2}
   do
    [ -d "${BINDIR}" ] && {
      echo "...bin dir"
      find -P "${BINDIR}" -type d \( -name 'lost+found' \) -prune -o \( -type f -o -type d \) ! \( -user "${USERNAME}" -a -group "${USERGROUP}" \) -exec chown -h "${USERNAME}:${USERGROUP}" '{}' \;
      find -P "${BINDIR}" -type d \( -name 'lost+found' \) -prune -o -type f -name "*.sh" ! -perm "${SCRIPTPERM}" -exec chmod "${SCRIPTPERM}" '{}' \;
      find -P "${BINDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -name "*.sh" ! -perm "${FILEPERM}" -exec chmod "${FILEPERM}" '{}' \;
      find -P "${BINDIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm "${DIRPERM}" -exec chmod "${DIRPERM}" '{}' \;
    }
  done
  ## --> mysql account files
  [ -f "${DIR}/.my.cnf" -o -f "${DIR}/.mysql_accounts" ] && {
    echo "...mysql account files"
    [ -f "${DIR}/.my.cnf" ] && chmod u=rw,go= "${DIR}/.my.cnf"
    [ -f "${DIR}/.mysql_accounts" ] && chmod u=rw,go= "${DIR}/.mysql_accounts"
  }
  ## --> mail account files
  [ -f "${DIR}/.mail_accounts" ] && {
    echo "...mail account files"
    [ -f "${DIR}/.mail_accounts" ] && chmod u=rw,go= "${DIR}/.mail_accounts"
  }
  ## --> www account files
  [ -f "${DIR}/.www_accounts" ] && {
    echo "...www account files"
    [ -f "${DIR}/.www_accounts" ] && chmod u=rw,go= "${DIR}/.www_accounts"
  }
  ## --> .subversion dir
  [ -d "${SVNDIR}" ] && {
    echo "...subversion dir"
    find -P "${SVNDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rw,og= -exec chmod u=rw,og= '{}' \;
    find -P "${SVNDIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,og= -exec chmod u=rwx-s,og=-s '{}' \;
  }
  ## --> Owner/Group
  echo "...owner/group"
  FINDCMD="find -P \"${DIR}\" -mindepth 1 -type d \( -name 'lost+found' -o -path \"${DIR}/.local\" -o -path \"${DIR}/setup_files\" -o -path \"${DIR}/mail\" -o -path \"${DIR}/www\" -o -path \"${SSHDIR}\" -o -path \"${BINDIR1}\" -o -path \"${BINDIR2}\" -o -path \"${SVNDIR}\" ${PRUNEJAIL} ${PRUNESVN} \) -prune -o \( -type f -o -type d \) ! \( -user \"${USERNAME}\" -a -group \"${USERGROUP}\" \) -exec chown -h \"${USERNAME}:${USERGROUP}\" '{}' \;"
  eval ${FINDCMD}
  ## --> Directories
  echo "...directory permissions"
  FINDCMD="find -P \"${DIR}\" -mindepth 1 -type d \( -name 'lost+found' -o -path \"${DIR}/.local\" -o -path \"${DIR}/setup_files\" -o -path \"${DIR}/mail\" -o -path \"${DIR}/www\" -o -path \"${SSHDIR}\" -o -path \"${BINDIR1}\" -o -path \"${BINDIR2}\" -o -path \"${SVNDIR}\" ${PRUNEJAIL} ${PRUNESVN} \) -prune -o -type d ! -perm \"${DIRPERM}\" -exec chmod \"${DIRPERM}\" '{}' \;"
  eval ${FINDCMD}
  ## --> Files (do at the end, so if cancelled the previous stuff is already secured)
  echo "...file permissions"
  FINDCMD="find -P \"${DIR}\" -mindepth 1 -type d \( -name 'lost+found' -o -path \"${DIR}/.local\" -o -path \"${DIR}/setup_files\" -o -path \"${DIR}/mail\" -o -path \"${DIR}/www\" -o -path \"${SSHDIR}\" -o -path \"${BINDIR1}\" -o -path \"${BINDIR2}\" -o -path \"${SVNDIR}\" ${PRUNEJAIL} ${PRUNESVN} \) -prune -o -type f ! \( -perm \"${FILEPERM}\" -o -path \"${DIR}/.my.cnf\" -o -path \"${DIR}/.mysql_accounts\" -o -path \"${DIR}/.mail_accounts\" -o -path \"${DIR}/.www_accounts\" \) -exec chmod \"${FILEPERM}\" '{}' \;"
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

  [ -z "${1}" ] && {
    echo "Usage: chmod_home_dir_user_group_only <user name> [<group name> [<dir pattern(s)>]]" 1>&2
    return 1 2>/dev/null || exit 1
  }

  local USERNAME
  local USERGROUP
  local DIRPATTERN

  USERNAME="${1}"
  USERGROUP="${2}"
  [ -z "${USERGROUP}" ] && USERGROUP="${USERNAME}"
  DIRPATTERN="${3}"
  [ -z "${DIRPATTERN}" ] && {
    [ "${USERNAME}" = 'root' ] && DIRPATTERN='/root'
    DIRPATTERN="${DIRPATTERN:+${DIRPATTERN} }/home/${USERNAME}"
  }
  set --

  for DIR in ${DIRPATTERN}
   do
    [ ! -d "${DIR}" ] && continue
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

  [ -z "${1}" ] && {
    echo "Usage: chmod_home_dir_user_only <user name> [<dir pattern(s)>]" 1>&2
    return 1 2>/dev/null || exit 1
  }

  local USERNAME
  local USERGROUP
  local DIRPATTERN

  USERNAME="${1}"
  USERGROUP="${1}"
  [ -z "${USERGROUP}" ] && USERGROUP="${USERNAME}"
  DIRPATTERN="${2}"
  [ -z "${DIRPATTERN}" ] && {
    [ "${USERNAME}" = 'root' ] && DIRPATTERN='/root'
    DIRPATTERN="${DIRPATTERN:+${DIRPATTERN} }/home/${USERNAME}"
  }
  set --

  for DIR in ${DIRPATTERN}
   do
    [ ! -d "${DIR}" ] && continue
    chmod_home_dir "${DIR}" "${USERNAME}" "${USERGROUP}" 'u=rw,go=' 'u=rwx-s,go=-s' 'u=rwx,go='
  done

  return 0
}


chmod_etc_custom_dir () {
  # parameters:
  # 1 = Dir Pattern

  [ -z "${1}" ] && {
    echo "Usage: chmod_etc_custom_dir <dir pattern(s)>" 1>&2
    return 1 2>/dev/null || exit 1
  }

  local DIRPATTERN

  DIRPATTERN="${1}"
  set --

  for DIR in ${DIRPATTERN}
   do
    [ ! -d "${DIR}" ] && continue
    echo "${0}: fixing owner/group and permissions in ${DIR}... (may take a while)"

    ## --> custom key files
    find -P "${DIR}" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/*.key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;

    ## --> Directories
    echo "...directory permissions"
    find -P "${DIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,go=rx -exec chmod u=rwx-s,go=rx-s '{}' \;
    ## --> Files (do at the end, so if cancelled the previous stuff is already secured)
    echo "...file permissions"
    find -P "${DIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! \( -path "${DIR}/*.key" \) ! -perm u=rw,go=r -exec chmod u=rw,go=r '{}' \;
  done

  return 0
}


chmod_etc_dir () {
  # parameters:
  # 1 = Dir Pattern

  [ -z "${1}" ] && {
    echo "Usage: chmod_etc_dir <dir pattern(s)>" 1>&2
    return 1 2>/dev/null || exit 1
  }

  local DIRPATTERN
  local SSLPRIVDIR

  DIRPATTERN="${1}"
  set --

  for DIR in ${DIRPATTERN}
   do
    [ ! -d "${DIR}" ] && continue
    echo "${0}: fixing owner/group and permissions in ${DIR}... (may take a while)"
    SSLPRIVDIR="${DIR}/ssl/private"
    POSTFIXDIR="${DIR}/postfix"
    LETSENCACCDIR="${DIR}/letsencrypt/accounts"

    ## --> letsencrypt account dir
    [ -d "${LETSENCACCDIR}" ] && {
      find -P "${LETSENCACCDIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,go= -exec chmod u=rwx-s,go=-s '{}' \;
      find -P "${LETSENCACCDIR}" -type d \( -name 'lost+found' \) -prune -o -type f -name "private_key.json" ! -perm u=r,go= -exec chmod u=r,go= '{}' \;
    }
    ## --> custom dir
    [ -d "${DIR}/custom" ] && {
      chmod_etc_custom_dir "${DIR}/custom"
    }
    ## --> ssl private dir
    [ -d "${SSLPRIVDIR}" ] && {
      find -P "${SSLPRIVDIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,go= -exec chmod u=rwx-s,go=-s '{}' \;
      find -P "${SSLPRIVDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    }
    ## --> cron.hourly dir (all files)
    [ -d "${DIR}/cron.hourly" ] && {
      find -P "${DIR}/cron.hourly" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rwx,go=rx -exec chmod u=rwx,go=rx '{}' \;
    }
    ## --> postfix dir (all files)
    [ -d "${POSTFIXDIR}" ] && {
      find -P "${POSTFIXDIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! -perm u=rw,g=r,o= -exec chmod u=rw,g=r,o= '{}' \;
    }
    ## --> bind key files
    [ -d "${DIR}/bind" ] && {
      find -P "${DIR}/bind" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/bind/*.key" ! -perm u=rw,g=r,o= -exec chmod u=rw,g=r,o= '{}' \;
      find -P "${DIR}/bind" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/bind/*.key" ! \( -user "root" -a -group "bind" \) -exec chown -h "root:bind" '{}' \; || true
    }
    ## --> dropbear key files
    [ -d "${DIR}/dropbear" ] && {
      find -P "${DIR}/dropbear" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/dropbear/dropbear_*_host_key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    }
    ## --> dropbear initramfs key files
    [ -d "${DIR}/dropbear-initramfs" ] && {
      find -P "${DIR}/dropbear-initramfs" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/dropbear-initramfs/dropbear_*_host_key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    }
    ## --> initramfs dropbear key files
    [ -d "${DIR}/initramfs-tools/etc/dropbear" ] && {
      find -P "${DIR}/initramfs-tools/etc/dropbear" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/initramfs-tools/etc/dropbear/dropbear_*_host_key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    }
    ## --> ssh key files
    [ -d "${DIR}/ssh" ] && {
      find -P "${DIR}/ssh" -type d \( -name 'lost+found' \) -prune -o -type f -path "${DIR}/ssh/ssh_host_*_key" ! -perm u=rw,go= -exec chmod u=rw,go= '{}' \;
    }

    ## --> Directories
    echo "...directory permissions"
    find -P "${DIR}" -type d \( -name 'lost+found' -o -path "${LETSENCACCDIR}" -o -path "${DIR}/custom" -o -path "${SSLPRIVDIR}" \) -prune -o -type d ! -perm u=rwx,go=rx -exec chmod u=rwx-s,go=rx-s '{}' \;
    ## --> Files (do at the end, so if cancelled the previous stuff is already secured)
    echo "...file permissions"
    find -P "${DIR}" -type d \( -name 'lost+found' -o -path "${LETSENCACCDIR}" -o -path "${DIR}/custom" -o -path "${SSLPRIVDIR}" -o -path "${DIR}/cron.hourly" -o -path "${POSTFIXDIR}" \) -prune -o -type f ! \( -path "${DIR}/bind/*.key" -o -path "${DIR}/dropbear/dropbear_*_host_key" -o -path "${DIR}/dropbear-initramfs/dropbear_*_host_key" -o -path "${DIR}/initramfs-tools/etc/dropbear/dropbear_*_host_key" -o -path "${DIR}/ssh/ssh_host_*_key" \) ! -perm u=rw,go=r -exec chmod u=rw,go=r '{}' \;
  done

  return 0
}


chmod_usr_dir () {
  # parameters:
  # 1 = Dir Pattern

  [ -z "${1}" ] && {
    echo "Usage: chmod_usr_dir <dir pattern(s)>" 1>&2
    return 1 2>/dev/null || exit 1
  }

  local DIRPATTERN
  local BINDIR

  DIRPATTERN="${1}"
  set --

  for DIR in ${DIRPATTERN}
   do
    [ ! -d "${DIR}" ] && continue
    echo "${0}: fixing owner/group and permissions in ${DIR}... (may take a while)"
    BINDIR="${DIR}/local/bin"

    ## --> bin .sh files
    [ -d "${BINDIR}" ] && {
      echo "...scripts"
      find -P "${BINDIR}" -maxdepth 1 -type f -name "*.sh" ! -perm u=rwx,go=rx -exec chmod u=rwx,go=rx '{}' \;
    }
    ## --> Directories
    echo "...directory permissions"
    find -P "${DIR}" -type d \( -name 'lost+found' \) -prune -o -type d ! -perm u=rwx,go=rx -exec chmod u=rwx-s,go=rx-s '{}' \;
    ## --> Files (do at the end, so if cancelled the previous stuff is already secured)
    echo "...file permissions"
    find -P "${DIR}" -type d \( -name 'lost+found' \) -prune -o -type f ! \( -perm u=rw,go=r -o -path "${BINDIR}/*.sh" \) -exec chmod u=rw,go=r '{}' \;
  done

  return 0
}
