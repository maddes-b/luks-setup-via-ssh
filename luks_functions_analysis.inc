#!/bin/false

echo 'Setting analysis functions...' 1>&2

. /root/bin/custom_functions.inc 2>/dev/null

EXTRASIZEBASE='5' ; # minimum reserve for device administration data
EXTRASIZEMDADM="$(( ${EXTRASIZEBASE} + 0 ))"
EXTRASIZELUKS="$(( ${EXTRASIZEBASE} + 0 ))"
EXTRASIZELVM="$(( ${EXTRASIZEBASE} + 5 ))" ; # additional space for pvresize to match extents


###
### Main Functions
###
print_subdevs_value () {
  # parameters:
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Suffix of Target Value Variable, e.g. SUBDEVS
  # 3 = Requested Types, e.g. mbr|gpt

  [ -z "${1}" ] && {
    echo "Usage: print_subdevs_value <var name> <val name suffix> [<device type>[|<device type>[|...]]]" 1>&2
    return 1 2>/dev/null || exit 1
  }

  local RESULT

  unset RESULT
  get_subdevs_value "${1}" "${2}" "${3}" ''
  echo "${RESULT}"

  return 0
}

determine_subdevs () {
  # parameters:
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Device to Investigate
  # 3 = Minimal Size for File System in MiB
  # 4 = Set mapper name as mount for file system

  local VARNAME
  local TESTDEV
  local FULLSIZE ; # will be used as a "fake global variable" to calculate the size for the subdevs
  local SETMAPPERMOUNT

  local DMNAME
  local MOUNTDEV
  local EXTRASIZE
  local UUID
  local UUID_SUB
  local LABEL
  local TYPE

  unset DMNAME MOUNTDEV
  unset EXTRASIZE
  unset UUID UUID_SUB LABEL TYPE

  [ -z "${1}" ] && {
    echo "Usage: determine_subdevs <var name> [<device> [<min size> [<set mapper mount>]]]" 1>&2
    return 1 2>/dev/null || exit 1
  }

  VARNAME="${1}"
  TESTDEV="${2}"
  FULLSIZE="${3}"
  SETMAPPERMOUNT="${4}"

  echo "echo 'Setting ${VARNAME} vars...' 1>&2"

  [ -z "${TESTDEV}" ] && {
    echo "unset ${VARNAME}MOUNT"
    echo "unset ${VARNAME}"
    return 0
  }

  [ -e "${TESTDEV}" ] || {
    echo "device ${TESTDEV} does NOT exist" 1>&2
    return 1 2>/dev/null || exit 1
  }

  blkid "${TESTDEV}" >/dev/null ; sync ; # make sure that super block is created and written

  get_dmname "${TESTDEV}"

  [ -n "${FULLSIZE}" ] && echo "# All sizes are in MiB (2^20 bytes)"

  ## Variable that holds the name under which the file system is mounted.
  ## When a mapper name available, then Debian recommends to use it for mounting.
  ## On Debian 4.0 this is even mandatory in /etc/fstab for cryptroot to work.
  [ -n "${SETMAPPERMOUNT}" -a -n "${DMNAME}" ] && MOUNTDEV="/dev/mapper/${DMNAME}"
  echo "${VARNAME}MOUNT='${MOUNTDEV:-${TESTDEV}}'"

  ## Variable that holds the best name for the file system, no matter what.
  ## Up to Debian 4.0 some commands cannot use a mapper name as a device, e.g. lvm
  [ -n "${DMNAME}" ] && TESTDEV="/dev/mapper/${DMNAME}"

  eval_blkid "${TESTDEV}"
  case "${TESTDEV}" in
    (*by-uuid*) TESTDEV="$(findfs UUID=${UUID})" ;;
    (*by-label*) TESTDEV="$(findfs LABEL=${LABEL})" ;;
    (*) ;;
  esac

  echo "${VARNAME}='${TESTDEV}'"
  echo "${VARNAME}TYPE='${TYPE}'"
  echo "${VARNAME}UUID='${UUID}'"
  [ -n "${UUID_SUB}" ] && echo "${VARNAME}UUID_SUB='${UUID_SUB}'"
  echo "${VARNAME}LABEL='${LABEL}'"
  [ -n "${FULLSIZE}" ] && {
    echo "${VARNAME}SIZEMIN='${FULLSIZE}'"
    case "${TYPE}" in
      (ext4) EXTRASIZE="$(( ( ${FULLSIZE} * 10 / 100 ) + ${EXTRASIZEBASE} ))" ;;
      (swap) EXTRASIZE="${EXTRASIZEBASE}" ;;
      (*) EXTRASIZE="$(( ( ${FULLSIZE} * 10 / 100 ) + ${EXTRASIZEBASE} ))" ;;
    esac
  }

  echo "${VARNAME} on ${TESTDEV} via ${TYPE}${FULLSIZE:+ and min size of ${FULLSIZE} MiB}" 1>&2

  get_subdevs '' "${TESTDEV}" "${FULLSIZE:+${EXTRASIZE}}"

  return 0
}


###
### Sub Functions
###
get_subdevs_value () {
  # parameters:
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Suffix of Target Value Variable, e.g. SUBDEVS
  # 3 = Requested Types, e.g. mbr|gpt
  # 4 = Level, e.g. _1

  local BASENAME
  local VALNAMES
  local TYPES
  local LEVEL

  local TYPE
  local FOUND
  local VALNAME
  local SUBLEVEL

  BASENAME="${1}"
  VALNAMES="${2}"
  [ -z "${VALNAMES}" ] && VALNAMES='DEV'
  TYPES="${3:-mbr|gpt}"
  LEVEL="${4}"

  [ -n "${LEVEL}" ] && {
    TYPE="$(eval echo "\"\${${BASENAME}${LEVEL}TYPE}\"")"
    FOUND=0
# eval hack, due to shells not being able to handle CASE ORs (=pipe symbol) in variables
    eval "case \"${TYPE}\" in
            (${TYPES}) FOUND=1 ;;
          esac"
    [ "${FOUND}" -ne 0 ] && {
      for VALNAME in ${VALNAMES}
       do
        [ "${VALNAME}" = 'DEV' ] && unset VALNAME
        if [ "${VALNAME}" = 'LEVEL' ] ; then
          RESULT="${RESULT:+${RESULT} }${LEVEL}"
        else
          RESULT="${RESULT:+${RESULT} }$(eval echo "\"\${${BASENAME}${LEVEL}${VALNAME}}\"")"
        fi
      done
    }
  }

  for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
   do
    get_subdevs_value "${BASENAME}" "${VALNAMES}" "${TYPES}" "${SUBLEVEL}"
  done

  return 0
}

get_subdevs () {
  local OLDPREFIX
  local DEVLIST
  local SIZE

  local LEVEL
  local TESTDEV
  local EXTRADEV

  local PREFIX
  local SUBLEVELS
  local NEWDEVLIST
  local DMNAME
  local DEV
  local LVM2_LV_UUID
  local LVM2_LV_NAME
  local LVM2_VG_NAME
  local LVM2_DEVICES
  local MD_LEVEL
  local MD_UUID
  local MD_NAME
  local MD_METADATA
  local MD_DATASTART
  local MD_DEVICES
  local MAINDEV
  local UUID
  local UUID_SUB
  local LABEL
  local TYPE

  unset PREFIX
  unset SUBLEVELS
  unset NEWDEVLIST
  unset DMNAME DEV
  unset LVM2_LV_UUID LVM2_LV_NAME LVM2_VG_NAME
  unset MD_LEVEL MD_UUID MD_NAME MD_METADATA MD_DATASTART
  unset MAINDEV
  unset UUID UUID_SUB LABEL TYPE

  OLDPREFIX="${1}"
  DEVLIST="${2}"
  SIZE="${3}"
  [ -z "${DEVLIST}" ] && return 0

  echo "${OLDPREFIX}_*: ${DEVLIST}" 1>&2

  [ -n "${SIZE}" ] && FULLSIZE="$(( ${FULLSIZE} + ${SIZE} ))"

  LEVEL=0
  for TESTDEV in ${DEVLIST}
   do
    [ -e "${TESTDEV}" ] || {
      echo "device ${TESTDEV} does NOT exist" 1>&2
      continue
    }

    LEVEL="$(( ${LEVEL} + 1 ))"

    PREFIX="${OLDPREFIX}_${LEVEL}"
    SUBLEVELS="${SUBLEVELS:+${SUBLEVELS} }${PREFIX}"

    get_dmname "${TESTDEV}"
    [ -n "${DMNAME}" ] && TESTDEV="/dev/mapper/${DMNAME}"

    # LVM
    unset EXTRADEV
    [ -n "${DMNAME}" -a "${DEBVERSION}" -le 4 ] && {
      echo "${TESTDEV}" | grep -q -F -e '-' && {
        EXTRADEV="${TESTDEV}"
        TESTDEV="/dev/$(echo "${DMNAME}" | sed -n -e 's#\([^-]\)-\([^-]\)#\1/\2# ; s#--#-# ; p ;')"
      }
    }
    lvm lvs --noheadings -o LV_NAME "${TESTDEV}" 1>/dev/null 2>&1 && {
      echo "${VARNAME}${PREFIX}='${TESTDEV}'"
      echo "${VARNAME}${PREFIX}TYPE='lvm'"
      eval_lvm "${TESTDEV}"
      echo "${VARNAME}${PREFIX}UUID='${LVM2_LV_UUID}'"
#     echo "${VARNAME}${PREFIX}LABEL='...'"
      echo "${VARNAME}${PREFIX}DMNAME='${DMNAME}'"
      echo "${VARNAME}${PREFIX}LV='${LVM2_LV_NAME}'"
      echo "${VARNAME}${PREFIX}VG='${LVM2_VG_NAME}'"
#     echo "${VARNAME}${PREFIX}VG_UUID='...vgs/vgdisplay...'"
#     echo "${VARNAME}${PREFIX}PV_UUID='...pvs/pvdisplay...'"
      unset NEWDEVLIST
      for DEV in ${LVM2_DEVICES}
       do
        get_dmname "${DEV}"
        [ -n "${DMNAME}" ] && DEV="/dev/mapper/${DMNAME}"
        NEWDEVLIST="${NEWDEVLIST:+${NEWDEVLIST} }${DEV}"
      done
      echo "${VARNAME}${PREFIX}SUBDEVS='${NEWDEVLIST}'"
      [ -n "${SIZE}" ] && {
        echo "${VARNAME}${PREFIX}SIZEMIN=\"\$(( \${${VARNAME}${OLDPREFIX}SIZEMIN} + ${SIZE} ))\""
        echo "# ATTENTION! Sizes for lvm physical devices below are only based on the above"
        echo "#            single logical volume instead of the actual pv usage. Please"
        echo "#            maintain the necessary pv sizes in ${VARNAME}${PREFIX}_?SIZEMIN"
        echo "#            with an extra ${EXTRASIZELVM} MiB buffer for each pv."
      }
      echo "${TESTDEV} via lvm on ${NEWDEVLIST}${SIZE:+ with min size of ${FULLSIZE} MiB}" 1>&2
      get_subdevs "${PREFIX}" "${NEWDEVLIST}" "${SIZE:+${EXTRASIZELVM}}"
      continue
    }
    [ -n "${EXTRADEV}" ] && TESTDEV="${EXTRADEV}"
    unset EXTRADEV

    # LUKS
    cryptsetup status "${TESTDEV}" 1>/dev/null 2>&1 && {
      echo "${VARNAME}${PREFIX}='${TESTDEV}'"
      echo "${VARNAME}${PREFIX}TYPE='luks'"
      NEWDEVLIST="$(cryptsetup status "${TESTDEV}" 2>/dev/null | sed -n -e 's#/dev/.static##' -e 's#^[[:space:]]\+device:[[:space:]]\+\([^[:space:]]\+\)[[:space:]]*#\1#p')"
      echo "${VARNAME}${PREFIX}UUID='$(cryptsetup luksUUID "${NEWDEVLIST}" 2>/dev/null)'"
#     echo "${VARNAME}${PREFIX}LABEL='...'"
      echo "${VARNAME}${PREFIX}DMNAME='${DMNAME}'"
      get_dmname "${NEWDEVLIST}"
      [ -n "${DMNAME}" ] && NEWDEVLIST="/dev/mapper/${DMNAME}"
      echo "${VARNAME}${PREFIX}SUBDEVS='${NEWDEVLIST}'"
      [ -n "${SIZE}" ] && echo "${VARNAME}${PREFIX}SIZEMIN=\"\$(( \${${VARNAME}${OLDPREFIX}SIZEMIN} + ${SIZE} ))\""
      echo "${TESTDEV} via luks on ${NEWDEVLIST}${SIZE:+ with min size of ${FULLSIZE} MiB}" 1>&2
      get_subdevs "${PREFIX}" "${NEWDEVLIST}" "${SIZE:+${EXTRASIZELUKS}}"
      continue
    }

    # MD Software RAID
    mdadm --detail "${TESTDEV}" --brief 1>/dev/null 2>&1 && {
      echo "${VARNAME}${PREFIX}='${TESTDEV}'"
      MAINDEV="${TESTDEV%%[0-9_]*}"
      MAINDEV="${MAINDEV%/}"
      if [ "${MAINDEV}/${TESTDEV##*[!0-9_]}" != "${TESTDEV}" ] ; then
        echo "${VARNAME}${PREFIX}ALT='${MAINDEV}/${TESTDEV##*[!0-9_]}'"
      else
        echo "${VARNAME}${PREFIX}ALT='${MAINDEV}${TESTDEV##*[!0-9_]}'"
      fi
      unset MAINDEV
      echo "${VARNAME}${PREFIX}TYPE='mdadm'"
      eval_mdadm "${TESTDEV}"
      echo "${VARNAME}${PREFIX}UUID='${MD_UUID}'"
      echo "${VARNAME}${PREFIX}LABEL='${MD_NAME}'"
      echo "${VARNAME}${PREFIX}METADATA='${MD_METADATA}'"
      MD_DATASTART="$(${CALL_ENV} mdadm --examine ${MD_DEVICES} | sed -n -e 's#^[[:space:]]*Data Offset : \([^[:space:]]\+\) sectors#\1#p' | sort -n -r | head -n 1)"
      if [ -n "${MD_DATASTART}" -a "${MD_DATASTART:-0}" -ne 0 ] ; then
        MD_DATASTART="$(( ${MD_DATASTART} / ( 2 * 1024 ) ))"
      else
        unset MD_DATASTART
      fi
      echo "${VARNAME}${PREFIX}DATASTART='${MD_DATASTART}'"
      echo "${VARNAME}${PREFIX}SUBDEVS='${MD_DEVICES}'"
#     echo "${VARNAME}${PREFIX}DMNAME='${DMNAME}'"
      echo "${VARNAME}${PREFIX}RLEVEL='${MD_LEVEL}'"
      [ -n "${SIZE}" ] && echo "${VARNAME}${PREFIX}SIZEMIN=\"\$(( \${${VARNAME}${OLDPREFIX}SIZEMIN} + ${SIZE} ))\""
      echo "${TESTDEV} via mdadm on ${MD_DEVICES}${SIZE:+ with min size of ${FULLSIZE} MiB}" 1>&2
      get_subdevs "${PREFIX}" "${MD_DEVICES}" "${SIZE:+$(( ${EXTRASIZEMDADM} + ${MD_DATASTART:-0} ))}"
      continue
    }

    # Disk Partitions (mbr/gpt; Rest is unknown)
    MAINDEV="$(echo "${TESTDEV}" | sed -n -e 's#\([^0-9]*\)\([0-9]*\)$#\1#p')"

    # GPT via fdisk
    ${CALL_ENV} fdisk -l "${MAINDEV}" 2>&1 1>/dev/null | grep -q -F -e 'WARNING: GPT (GUID Partition Table)' && {
      echo "${VARNAME}${PREFIX}='${TESTDEV}'"
      echo "${VARNAME}${PREFIX}TYPE='gpt'"
      eval_blkid "${TESTDEV}"
      echo "${VARNAME}${PREFIX}UUID='${UUID}'"
      [ -n "${UUID_SUB}" ] && echo "${VARNAME}${PREFIX}UUID_SUB='${UUID_SUB}'"
      echo "${VARNAME}${PREFIX}LABEL='${LABEL}'"
      echo "${VARNAME}${PREFIX}SUBDEVS='${MAINDEV}'"
      echo "${VARNAME}${PREFIX}SUBPART='$(echo "${TESTDEV}" | sed -n -e 's#\([^0-9]*\)\([0-9]*\)$#\2#p')'"
      [ -n "${SIZE}" ] && echo "${VARNAME}${PREFIX}SIZEMIN=\"\$(( \${${VARNAME}${OLDPREFIX}SIZEMIN} + ${SIZE} ))\""
      echo "${TESTDEV} via gpt${SIZE:+ with min size of ${FULLSIZE} MiB}" 1>&2
      continue
    }

    # MBR via fdisk
    ${CALL_ENV} fdisk -l "${MAINDEV}" 2>&1 1>/dev/null | grep -q -F -e "doesn't contain a valid partition table" || {
      echo "${VARNAME}${PREFIX}='${TESTDEV}'"
      echo "${VARNAME}${PREFIX}TYPE='mbr'"
      eval_blkid "${TESTDEV}"
      echo "${VARNAME}${PREFIX}UUID='${UUID}'"
      [ -n "${UUID_SUB}" ] && echo "${VARNAME}${PREFIX}UUID_SUB='${UUID_SUB}'"
      echo "${VARNAME}${PREFIX}LABEL='${LABEL}'"
      echo "${VARNAME}${PREFIX}SUBDEVS='${MAINDEV}'"
      echo "${VARNAME}${PREFIX}SUBPART='$(echo "${TESTDEV}" | sed -n -e 's#\([^0-9]*\)\([0-9]*\)$#\2#p')'"
      [ -n "${SIZE}" ] && echo "${VARNAME}${PREFIX}SIZEMIN=\"\$(( \${${VARNAME}${OLDPREFIX}SIZEMIN} + ${SIZE} ))\""
      echo "${TESTDEV} via mbr${SIZE:+ with min size of ${FULLSIZE} MiB}" 1>&2
      continue
    }

    # unknown
    echo "${VARNAME}${PREFIX}='${TESTDEV}'"
    echo "${VARNAME}${PREFIX}TYPE='unknown'"
    echo "${TESTDEV} via unknown" 1>&2
    continue
  done ; # for-loop

  [ -n "${SUBLEVELS}" ] && echo "${VARNAME}${OLDPREFIX}SUBLEVELS='${SUBLEVELS}'"

  return 0
}

show_temp_suggestions () {
  # 1 = Base Name of Old Variable, e.g. OLDROOT
  # 2 = Base Name of New Variable, e.g. TEMPEX
  # 3 = Wanted Device Number, e.g. 9
  # 4 = Wanted Size of File System (default is <OLDBASENAME>SIZEMIN)

  local OLDBASENAME
  local NEWBASENAME
  local BASEDEVNO
  local ORGBASESIZE
  local BASESIZE

  local SIZE
  local SUBSIZE
  local LEVELS
  local LEVEL
  local DEVS
  local DEV
  local DEVNO
  local TYPE
  local SUBDEVS
  local SUBTYPE
  local SECSIZE
  local MDADM
  local MDADMCOUNT
  local RLEVEL
  local LUKSCIPHER

  [ -z "${2}" ] && {
    echo "Usage: show_temp_suggestions <old var name> <new var name> [<part/mdadm device number> [<wanted size>]]" 1>&2
    return 1 2>/dev/null || exit 1
  }

  OLDBASENAME="${1}"
  NEWBASENAME="${2}"
  BASEDEVNO="${3}"
  ORGBASESIZE="${4}"
  BASESIZE="${ORGBASESIZE:-0}"
  [ "${BASESIZE}" -eq 0 ] && BASESIZE="$(eval echo "\"\${${OLDBASENAME}SIZEMIN}\"")"
  BASESIZE="${BASESIZE:-0}"

  unset DEVS
  unset SUBDEVS
  unset SUBTYPE
  unset MDADM

  # file system
  SIZE="${BASESIZE}"
  [ "${NEWBASENAME}" != 'MINIDEB' ] && {
    LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'lvm')"
    [ -n "${LEVELS}" ] && {
      SIZE=0
      for LEVEL in ${LEVELS}
       do
        for SUBLEVEL in $(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBLEVELS}\"")
         do
          SUBSIZE="$(eval echo "\"\${${OLDBASENAME}${SUBLEVEL}SIZEMIN}\"")"
          [ -n "${SUBSIZE}" ] && SUBSIZE="$(( ${SUBSIZE} - ${EXTRASIZELVM} ))"
          SIZE="$(( ${SIZE} + ${SUBSIZE:-0} ))"
        done
      done
    }
  }
  echo ''
  echo "### Create device(s) for temporary storage to hold data of ${SIZE} MiB"
  echo -n "### while keeping the exact same build structure of ${OLDBASENAME}"
  [ "${ORGBASESIZE:-0}" -eq 0 ] && echo -n ' to re-use identical sizes'
  printf '\n'
  echo '### providing the same level of reliability and security'

  # determine size of lowest sub device (normally partitions)
  # buffer for filesystem (assume ext4 for swap)
  case "$(eval echo "\"\${${OLDBASENAME}TYPE}\"")" in
   (ext4|swap) SIZE="$(( ( ${SIZE} * 110 / 100 ) + ${EXTRASIZEBASE} ))" ;;
   (*) SIZE="$(( ( ${SIZE} * 110 / 100 ) + ${EXTRASIZEBASE} ))" ;;
  esac
  [ "${NEWBASENAME}" != 'MINIDEB' ] && {
    LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'lvm')"
    [ -n "${LEVELS}" ] && SIZE="$(( ${SIZE} + ${EXTRASIZELVM} ))"
  }
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'luks')"
  [ -n "${LEVELS}" ] && SIZE="$(( ${SIZE} + ${EXTRASIZELUKS} ))"
  # check for raid
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mdadm')"
  [ -n "${LEVELS}" ] && {
    MDADM='yes'
    SIZE="$(( ${SIZE} + ${EXTRASIZEMDADM} ))"
    for LEVEL in ${LEVELS}
     do
      SUBSIZE="$(eval echo "\"\${${OLDBASENAME}${LEVEL}DATASTART}\"")"
      SIZE="$(( ${SIZE} + ${SUBSIZE:-0} ))"
      break
    done
  }

  # partitions
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mbr|gpt')"
  [ -n "${LEVELS}" ] && {
    unset SUBDEVS
    unset DEVS
  }
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mbr')"
  [ -n "${LEVELS}" ] && {
    DEVNO="${BASEDEVNO:-6}"
    echo "## Create mbr partition(s) via fdisk at the end of the devices, e.g. as partition ${DEVNO}"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBDEVS}\"")"
      echo -n "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${DEV}' ; # ${DEV}${DEVNO} with ${SIZE} MiB${MDADM:+ and type FD}, e.g. START sector is last aligned sector minus"
      for SECSIZE in 512 1024 2048 4096
       do
        SUBSIZE="$(( ( ${SIZE} * 1024 * 1024 ) / ${SECSIZE} ))"
        echo -n " ${SUBSIZE}s(${SECSIZE})"
      done
      echo -n ', and for END sector use '
      [ "${DEBVERSION}" -ge 6 ] && {
        SUBSIZE="$(( ${SIZE} * 1024 ))"
        echo -n "+${SUBSIZE}K or "
      }
      echo '+(sectors-1)'
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
    done
    [ "${DEVNO}" -ge 5 ] && {
      echo "  If you have to enlarge the extended partiton(s) then try to use a primary"
      echo "  partition (<=4) if enough slots are free. Otherwise note down all logical"
      echo "  partitions, delete all, recreate a larger extended parititon where START of it"
      echo "  will be the old start sector minus above mentioned sectors, recreate all"
      echo "  previous logical partitions, then create temporary storage and check its size"
      echo "  in sectors."
      echo "  Do *NOT* remove any signatures, otherwise you will destroy your data."
      echo "  Just re-check your planning of the preparations phase."
    }
    SUBTYPE='mbr'
  }
  #
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'gpt')"
  [ -n "${LEVELS}" ] && {
    DEVNO="${BASEDEVNO:-9}"
    echo "## Create gpt partition(s) via gdisk, e.g. as partition ${DEVNO}"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBDEVS}\"")"
      echo "gdisk '${DEV}' ; # ${DEV}${DEVNO} with ${SIZE} MiB${MDADM:+ and type FD00}, e.g. start is -${SIZE}M"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
    done
    SUBTYPE='gpt'
  }
  echo '## partprobe or reboot to make new partition(s) known to kernel'
  echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
  echo '#'
  echo 'for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'

  # mdadm
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mdadm')"
  [ -n "${LEVELS}" ] && {
    MDADMCOUNT=0
    for DEV in ${DEVS}
     do
      MDADMCOUNT="$(( ${MDADMCOUNT} + 1 ))"
    done
    unset LEVEL
    unset RLEVEL
    for LEVEL in ${LEVELS}
     do
      RLEVEL="$(eval echo "\"\${${OLDBASENAME}${LEVEL}RLEVEL}\"")"
      break
    done
    [ -z "${RLEVEL}" ] && RLEVEL='raid1'
    DEVNO="${BASEDEVNO:-${DEVNO}}"
    DEV="/dev/md${DEVNO}"
    echo "## Combine partitions to an mdadm raid, e.g. ${DEV}"
    echo "mdadm --zero-superblock ${DEVS}"
    echo "# take options from mdadm --detail '$(eval echo "\"\${${OLDBASENAME}${LEVEL}}\"")'"
    echo "mdadm --create '${DEV}' -l ${RLEVEL} -n ${MDADMCOUNT} $([ "${DEBVERSION}" -ge 7 ] && echo '-b internal ')<more options> ${DEVS}"
    echo '## Rebuild /etc/mdadm/mdadm.conf'
    echo '/usr/share/mdadm/mkconf >/etc/mdadm/mdadm.conf.new'
    echo 'diff -s -u /etc/mdadm/mdadm.conf /etc/mdadm/mdadm.conf.new'
    echo 'mv /etc/mdadm/mdadm.conf.new /etc/mdadm/mdadm.conf'
    SUBDEVS="${DEVS}"
    SUBTYPE='mdadm'
    DEVS="${DEV}"
    MDADM="${DEVS}"
  }

  # luks
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'luks')"
  [ -n "${LEVELS}" ] && {
    DEVNO="${DEVS#/dev/}_crypt"
    if [ "${DEBVERSION}" -le 5 ] ; then
      LUKSCIPHER='--cipher=aes-cbc-essiv:sha256 --key-size=256'
    else
      LUKSCIPHER='--cipher=aes-xts-plain64 --key-size=512'
    fi
    echo '## Create an encrypted device inside it'
    echo "cryptsetup --verbose --verify-passphrase --hash=sha512 ${LUKSCIPHER} luksFormat '${DEVS}'"
    [ -n "${MDADM}" ] && {
      echo "mdadm_wait '${MDADM}' ; printf '\\a' ; # TAKES TIME!!!"
      echo '## Populate /dev/disk/by-uuid'
      echo "mdadm -S '${MDADM}'"
      echo "mdadm -A '${MDADM}'"
    }
    echo '# update /etc/crypttab'
    if [ "${DEBVERSION}" -le 5 ] ; then
      echo "echo \"$([ "${DEBVERSION}" -le 4 ] && echo '#')${DEVNO} ${DEVS} none luks$([ "${DEBVERSION}" -ge 5 ] && echo ',noauto')\" >>/etc/crypttab"
      echo 'nano /etc/crypttab'
      echo '#'
    else
      echo "echo \"${DEVNO} UUID=\$(cryptsetup luksUUID ${DEVS}) none luks,noauto\" >>/etc/crypttab"
      echo 'nano /etc/crypttab'
      echo '#'
      [ "${DEBVERSION}" -le 6 ] && {
        echo "partprobe || blockdev --rereadpt ${DEVS} ; # otherwise reboot"
        echo '#'
        echo 'for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
      }
    fi
    if [ "${DEBVERSION}" -le 4 ] ; then
      echo "cryptsetup luksOpen '${DEVS}' '${DEVNO}'"
    else
      echo "cryptdisks_start '${DEVNO}' || cryptsetup luksOpen '${DEVS}' '${DEVNO}'"
    fi
    SUBDEVS="${DEVS}"
    SUBTYPE='luks'
    DEVS="/dev/mapper/${DEVNO}"
  }

  # lvm
  if [ "${NEWBASENAME}" = 'MINIDEB' ] ; then
    echo '## Avoid lvm for temporary storage'
  else
    LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'lvm')"
    [ -n "${LEVELS}" ] && {
      for LEVEL in ${LEVELS}
       do
        VG="$(eval echo "\"\${${OLDBASENAME}${LEVEL}VG}\"")"
        break
      done
      [ -z "${VG}" ] && VG="${HOSTNAME}_${NEWBASENAME}"
      echo "## Add new physical volume to volume group"
      echo "lvm pvcreate ${DEVS}"
      echo "lvm vgextend '${VG}' ${DEVS}"
      echo "lvm vgdisplay -v --units m '${VG}'"
      echo "## Add dummy logical volume for determine_subdevs"
      DEVNO="${NEWBASENAME}"
      echo "lvm lvcreate -n '${DEVNO}' -l 1 '${VG}' ${DEVS}"
      echo "lvm lvs -o +DEVICES --units m '${VG}'"
      if [ "${DEBVERSION}" -le 4 ] ; then
        echo "lvm lvdisplay -v -m"
      else
        echo "lvm pvdisplay -v -m ${DEVS} ; #or lvm pvs -v --units m --segments ${DEVS}"
      fi
      SUBDEVS="${DEVS}"
      SUBTYPE='lvm'
      DEVS="/dev/${VG}/${DEVNO}"
    }
  fi

  # file system
  echo '## Create file system'
  TYPE="$(eval echo "\"\${${OLDBASENAME}TYPE}\"")"
  [ "${TYPE}" = 'swap' ] && TYPE="${OLDROOTTYPE}"
  echo "mkfs.${TYPE} '${DEVS}' ; printf '\\a'"
  [ -n "${MDADM}" ] && {
    echo "mdadm_wait '${MDADM}' ; printf '\\a' ; # TAKES TIME!!!"
  }

  # script
  echo '## Set variables for temporary storage'
  echo "${NEWBASENAME}='${DEVS}'"
  echo "${NEWBASENAME}_1='${DEVS}'"
  echo "${NEWBASENAME}_1TYPE='${SUBTYPE}'"
  echo "${NEWBASENAME}_1SUBDEVS='${SUBDEVS}'"
  echo ''

  return 0
}

show_newroot_suggestions () {
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Wanted MDADM/Device Number, e.g. 6
  # 3 = Wanted Device Number, e.g. 6
  # 4 = Wanted encryption key, e.g. none, /dev/[u]random, etc.

  local OLDBASENAME
  local NEWBASENAME
  local BASEMDADM
  local BASEDEVNO
  local BASEKEY

  local SIZE
  local SUBSIZE
  local LEVELS
  local LEVEL
  local PARTTYPE
  local DEV
  local DEVS
  local DEVNO
  local MDADM
  local MDADMCOUNT
  local RLEVEL

  local TYPE
  local NEWSWAPCRYPTKEY
  local NEWSWAPCRYPTSCRIPT
  local NEWSWAPCRYPTPWOPTS
  local NEWSWAPCRYPTKEYOPTS
  local LUKSCIPHER

  local BOOTSIZE
  local BOOTDEVS
  local BOOTMDADM
  local BOOTSUBTYPE
  local BOOTSUBDEVS

  [ -z "${1}" ] && {
    echo "Usage: show_newroot_suggestions <old var name> [<mdadm dev number> [<part dev number> [<luks key>]]]" 1>&2
    return 1 2>/dev/null || exit 1
  }

  OLDBASENAME="${1}"
  NEWBASENAME="NEWROOT"
  BASEMDADM="${2}"
  BASEDEVNO="${3}"
  BASEKEY="${4:-none}"

  unset DEVS
  unset MDADM

  unset BOOTSIZE
  unset BOOTDEVS
  unset BOOTMDADM
  unset BOOTSUBTYPE
  unset BOOTSUBDEVS

  unset TYPE
  unset NEWSWAPCRYPTKEY
  unset NEWSWAPCRYPTSCRIPT
  unset NEWSWAPCRYPTPWOPTS
  unset NEWSWAPCRYPTKEYOPTS

  NEWSWAPCRYPTKEY="${BASEKEY:-none}"
### TODO
  case "${NEWSWAPCRYPTKEY}" in
    ('none')
      TYPE='luks'
      NEWSWAPCRYPTPWOPTS='--verify-passphrase --hash=sha512'
      ;;
    (/*)
      TYPE='swap'
      ;;
    (*)
      TYPE='luks'
      NEWSWAPCRYPTSCRIPT='/lib/cryptsetup/scripts/decrypt_derived'
      NEWSWAPCRYPTKEYOPTS='--key-file=-'
      ;;
  esac

  # file system
  echo ''
  SIZE="$(eval echo "\"\${${OLDBASENAME}SIZEMIN}\"")"
  [ -z "${OLDBOOT}" ] && {
    BOOTSIZE='512'
    echo "### Create device(s) for new UNENCRYPTED BOOT to hold data of ${BOOTSIZE} MiB"
  }
  echo "### Create device(s) for new ENCRYPTED ROOT to hold data of ${SIZE} MiB"

  # check for raid
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mdadm')"
  [ -n "${BASEMDADM}${LEVELS}" ] && {
    MDADM='yes'
    [ -z "${OLDBOOT}" ] && {
      BOOTMDADM='yes'
    }
  }

  # partitions
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mbr|gpt')"
  [ -n "${LEVELS}" ] && {
    unset SUBDEVS
    unset DEVS
    unset BOOTSUBDEVS
    unset BOOTDEVS
  }
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mbr')"
  [ -n "${LEVELS}" ] && {
    for LEVEL in ${LEVELS}
     do
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBPART}\"")}"
      break
    done
    [ -z "${OLDBOOT}" ] && {
      echo "## Create mbr partition(s) for new BOOT via fdisk at the beginning of the devices, e.g. as partition ${DEVNO}"
      for LEVEL in ${LEVELS}
       do
        DEV="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBDEVS}\"")"
        echo -n "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${DEV}' ; # ${DEV}${DEVNO} with ${BOOTSIZE} MiB${BOOTMDADM:+ and type FD}, e.g. START sector is first aligned sector, and for END sector use "
        [ "${DEBVERSION}" -ge 6 ] && {
          SUBSIZE="$(( ${BOOTSIZE} * 1024 ))"
          echo -n "+${SUBSIZE}K or "
        }
        echo -n '+(sectors-1)'
        for SECSIZE in 512 1024 2048 4096
         do
          SUBSIZE="$(( ( ${BOOTSIZE} * 1024 * 1024 ) / ${SECSIZE} ))"
          echo -n " ${SUBSIZE}s(${SECSIZE})"
        done
        printf '\n'
        BOOTSUBDEVS="${BOOTSUBDEVS:+${BOOTSUBDEVS} }${DEV}"
        BOOTDEVS="${BOOTDEVS:+${BOOTDEVS} }${DEV}${DEVNO}"
      done
      DEVNO="$(( ${DEVNO} + 1 ))"
      BOOTSUBTYPE='mbr'
    }
    echo "## Create mbr partition(s) for new root via fdisk, e.g. as partition ${DEVNO}"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBDEVS}\"")"
      SIZE="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SIZEMIN}\"")"
      echo "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${DEV}' ; # ${DEV}${DEVNO} with all available space${MDADM:+ and type FD}, but at least ${SIZE} MiB, e.g. "
      [ "${DEBVERSION}" -ge 6 ] && {
        SUBSIZE="$(( ${SIZE} * 1024 ))"
        echo -n "+${SUBSIZE}K or "
      }
      echo -n '+(sectors-1)'
      for SECSIZE in 512 1024 2048 4096
       do
        SUBSIZE="$(( ( ${SIZE} * 1024 * 1024 ) / ${SECSIZE} ))"
        echo -n " ${SUBSIZE}s(${SECSIZE})"
      done
      printf '\n'
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
    done
    SUBTYPE='mbr'
  }
  #
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'gpt')"
  [ -n "${LEVELS}" ] && {
    for LEVEL in ${LEVELS}
     do
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBPART}\"")}"
      break
    done
    [ -z "${OLDBOOT}" ] && {
      echo "## Create gpt partition(s) for new BOOT via gdisk, e.g. as partition ${DEVNO}"
      for LEVEL in ${LEVELS}
       do
        DEV="$(eval echo "\${${OLDBASENAME}${LEVEL}SUBDEVS}")"
        echo "gdisk '${DEV}' ; # ${DEV}${DEVNO} with +${BOOTSIZE}M${BOOTMDADM:+ and type FD00}"
        BOOTSUBDEVS="${BOOTSUBDEVS:+${BOOTSUBDEVS} }${DEV}"
        BOOTDEVS="${BOOTDEVS:+${BOOTDEVS} }${DEV}${DEVNO}"
      done
      DEVNO="$(( ${DEVNO} + 1 ))"
    }
    echo "## Create gpt partition(s) for new root via gdisk, e.g. as partition ${DEVNO}"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBDEVS}\"")"
      SIZE="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SIZEMIN}\"")"
      echo "gdisk '${DEV}' ; # ${DEV}${DEVNO} with all available space${MDADM:+ and type FD00}, but at least +${SIZE}M"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
    done
    SUBTYPE='gpt'
  }
  echo '## partprobe or reboot to make new partition(s) known to kernel'
  echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
  echo '#'
  echo 'for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
  echo '## Overwrite all unencrypted data that may remain on the mbr/gpt partition(s)'
  for DEV in ${BOOTDEVS} ${DEVS}
   do
    echo "dd if=/dev/urandom of='${DEV}' bs=1024k ; \\"
  done
  echo "printf '\\a' ; # TAKES TIME!!!"

  # mdadm
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mdadm')"
  [ -n "${BASEMDADM}${LEVELS}" ] && {
    MDADMCOUNT=0
    for DEV in ${DEVS}
     do
      MDADMCOUNT="$(( ${MDADMCOUNT} + 1 ))"
    done
    unset LEVEL
    unset RLEVEL
    [ -n "${LEVELS}" ] && {
      for LEVEL in ${LEVELS}
       do
        RLEVEL="$(eval echo "\"\${${OLDBASENAME}${LEVEL}RLEVEL}\"")"
        [ -z "${BASEMDADM}" ] && {
          DEVNO="$(eval echo "\"\${${OLDBASENAME}${LEVEL}}\"")"
          DEVNO="${DEVNO##*/}"
          DEVNO="$(echo "${DEVNO}" | sed -n -e 's#[^0-9]*\(.*\)$#\1#p')"
        }
        break
      done
    }
    [ -z "${RLEVEL}" ] && RLEVEL='raid1'
    DEVNO="${BASEMDADM:-${DEVNO}}"
    [ -z "${LEVELS}" -o "${MDADMCOUNT:-0}" -le 1 ] && {
      echo "## To add more disks to the raid: clone the harddisk structure, for gpt change GUIDs, add to raid array creation"
      echo "# for mbr disks"
      echo "sfdisk --dump ${SUBDEVS%% *} | sfdisk /dev/sdX"
      echo "# for gpt disks"
      echo "sgdisk --replicate=/dev/sdX ${SUBDEVS%% *}"
      echo "sgdisk --randomize-guids /dev/sdX"
      echo "# to add later to the array (IF SUPPORTED! NOT RECOMMENDED!)"
      echo "mdadm --add /dev/mdX /dev/sdXn"
      echo "mdadm --grow --raid-devices=Y /dev/mdX"
    }
    [ -z "${OLDBOOT}" ] && {
      DEV="/dev/md${DEVNO}"
      echo "## Combine partitions for new BOOT to an mdadm level 1 raid, e.g. ${DEV}"
      echo "mdadm --zero-superblock ${BOOTDEVS}$([ "${MDADMCOUNT:-0}" -le 1 ] && echo ' /dev/sdZn')"
      echo "mdadm --create '${DEV}' -l raid1 -n $(if [ "${MDADMCOUNT:-0}" -le 1 ]; then echo 'Y'; else echo "${MDADMCOUNT}"; fi) $([ "${DEBVERSION}" -ge 7 ] && echo '-b internal ')<more options> ${BOOTDEVS}$([ "${MDADMCOUNT:-0}" -le 1 ] && echo ' /dev/sdZn')"
      BOOTSUBDEVS="${BOOTDEVS}"
      BOOTSUBTYPE='mdadm'
      BOOTDEVS="${DEV}"
      BOOTMDADM="${BOOTDEVS}"
      DEVNO="$(( ${DEVNO} + 1 ))"
    }
    DEV="/dev/md${DEVNO}"
    echo "## Combine partitions for new ROOT to an mdadm raid, e.g. ${DEV}"
    echo "mdadm --zero-superblock ${DEVS}$([ "${MDADMCOUNT:-0}" -le 1 ] && echo ' /dev/sdXn')"
    [ -n "${LEVEL}" ] && {
      echo "# take options from mdadm --detail '$(eval echo "\"\${${OLDBASENAME}${LEVEL}}\"")' BEFORE removing old root"
    }
    echo "mdadm --create '${DEV}' -l ${RLEVEL} -n $(if [ "${MDADMCOUNT:-0}" -le 1 ]; then echo 'Y'; else echo "${MDADMCOUNT}"; fi) $([ "${DEBVERSION}" -ge 7 ] && echo '-b internal ')<more options> ${DEVS}$([ "${MDADMCOUNT:-0}" -le 1 ] && echo ' /dev/sdXn')"
    echo '## Rebuild /etc/mdadm/mdadm.conf'
    echo '/usr/share/mdadm/mkconf >/etc/mdadm/mdadm.conf.new'
    echo 'diff -s -u /etc/mdadm/mdadm.conf /etc/mdadm/mdadm.conf.new'
    echo 'mv /etc/mdadm/mdadm.conf.new /etc/mdadm/mdadm.conf'
    SUBDEVS="${DEVS}"
    SUBTYPE='mdadm'
    DEVS="${DEV}"
    MDADM="${DEVS}"
  }

  # luks
  echo '## Create an encrypted device for new root'
  DEVNO="${DEVS#/dev/}_crypt"
  #
  if [ "${DEBVERSION}" -le 5 ] ; then
    LUKSCIPHER='--cipher=aes-cbc-essiv:sha256 --key-size=256'
  else
    LUKSCIPHER='--cipher=aes-xts-plain64 --key-size=512'
  fi
  if [ -n "${NEWSWAPCRYPTSCRIPT}" ] ; then
    echo "${NEWSWAPCRYPTSCRIPT} ${NEWSWAPCRYPTKEY} | cryptsetup --verbose ${NEWSWAPCRYPTKEYOPTS:+${NEWSWAPCRYPTKEYOPTS} }${LUKSCIPHER} luksFormat '${DEVS}'"
  else
    echo "cryptsetup --verbose ${NEWSWAPCRYPTPWOPTS:+${NEWSWAPCRYPTPWOPTS} }${LUKSCIPHER} luksFormat '${DEVS}'"
  fi
  [ -n "${MDADM}" ] && {
    echo "mdadm_wait '${MDADM}' ; printf '\\a' ; # TAKES TIME!!!"
    echo '## Populate /dev/disk/by-uuid'
    echo "mdadm -S '${DEV}'"
    echo "mdadm -A '${DEV}'"
  }
  echo '# update /etc/crypttab'
  if [ "${DEBVERSION}" -le 5 ] ; then
    echo "echo \"$([ "${DEBVERSION}" -le 4 ] && echo '#')${DEVNO} ${DEVS} ${NEWSWAPCRYPTKEY} ${TYPE}${NEWSWAPCRYPTSCRIPT:+,keyscript=${NEWSWAPCRYPTSCRIPT}}$([ "${TYPE}" = 'swap' ] && echo ",offset=8")$([ "${DEBVERSION}" -ge 5 ] && echo ',noauto')\" >>/etc/crypttab"
    echo 'nano /etc/crypttab'
    echo '#'
  else
    echo "echo \"${DEVNO} UUID=\$(cryptsetup luksUUID ${DEVS}) ${NEWSWAPCRYPTKEY} ${TYPE}${NEWSWAPCRYPTSCRIPT:+,keyscript=${NEWSWAPCRYPTSCRIPT}}$([ "${TYPE}" = 'swap' ] && echo ",offset=8"),noauto\" >>/etc/crypttab"
    echo 'nano /etc/crypttab'
    echo '#'
    [ "${DEBVERSION}" -le 6 ] && {
      echo "partprobe || blockdev --rereadpt ${DEVS} ; # otherwise reboot"
      echo '#'
      echo 'for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
    }
  fi
  if [ "${DEBVERSION}" -le 4 ] ; then
    echo "cryptsetup luksOpen '${DEVS}' '${DEVNO}'"
  else
    echo "cryptdisks_start '${DEVNO}' || cryptsetup luksOpen '${DEVS}' '${DEVNO}'"
  fi
  SUBDEVS="${DEVS}"
  SUBTYPE='luks'
  DEVS="/dev/mapper/${DEVNO}"

  # lvm
  echo '## Create/extend lvm group with encrypted device'
  echo '## NOTE: here the VG name could be switched/changed for the new encrypted root system'
  DEV="${HOSTNAME}"
  echo "lvm pvcreate ${DEVS}"
  echo "lvm vgcreate '${DEV}' ${DEVS} || lvm vgextend '${DEV}' ${DEVS}"
  echo "lvm vgdisplay -v --units m '${DEV}'"
  echo "## Add dummy logical volume for determine_subdevs"
  DEVNO="${NEWBASENAME}"
  echo "lvm lvcreate -n '${DEVNO}' -l 1 '${DEV}' ${DEVS}"
  SUBDEVS="${DEVS}"
  SUBTYPE='lvm'
  DEVS="/dev/${DEV}/${DEVNO}"

  # file system
  [ -z "${OLDBOOT}" ] && {
    echo '## Create file system for new BOOT'
    echo "mkfs.ext2 ${BOOTDEVS} ; printf '\\a'"
  }
  echo '## Create file system for dummy new ROOT'
  echo "mkfs.$(eval echo "\"\${${OLDBASENAME}TYPE}\"") ${DEVS} ; printf '\\a'"

  # sync
  echo '## Sync all devices'
  echo 'sync'
  [ -n "${MDADM}${BOOTMDADM}" ] && {
    echo "mdadm_wait ${BOOTMDADM:+${BOOTMDADM} }${MDADM} ; printf '\\a' ; # TAKES TIME!!!"
  }

  # script
  echo '## Set variables for new BOOT'
  if [ -z "${OLDBOOT}" ] ; then
    echo "NEWBOOT='${BOOTDEVS}'"
    echo "NEWBOOT_1TYPE='${BOOTSUBTYPE}'"
    echo "NEWBOOT_1SUBDEVS='${BOOTSUBDEVS}'"
  else
    echo "NEWBOOT='${OLDBOOT}'"
    echo "NEWBOOT_1TYPE='${OLDBOOT_1TYPE}'"
    echo "NEWBOOT_1SUBDEVS='${OLDBOOT_1SUBDEVS}'"
  fi
  echo '## Set variables for new root'
  echo "${NEWBASENAME}='${DEVS}'"
  echo "${NEWBASENAME}_1TYPE='${SUBTYPE}'"
  echo "${NEWBASENAME}_1SUBDEVS='${SUBDEVS}'"
  echo ''

  return 0
}

show_clear_suggestions () {
  # 1 = Base Name of Variable, e.g. TEMPEX
  # 2 = Wanted Device Number, e.g. 6

  local BASENAME
  local BASEDEVNO

  local LEVELS
  local LEVEL
  local DEV
  local DEVS
  local DEVNO
  local SUBDEVS

  [ -z "${1}" ] && {
    echo "Usage: show_clear_suggestions <var name>" 1>&2
    return 1 2>/dev/null || exit 1
  }

  BASENAME="${1}"
  BASEDEVNO="${2}"

  echo ''
  echo "### Create temporary device(s) to overwrite unencrypted old data of free hdd space"

  # partitions
  unset DEVS
  unset SUBDEVS
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'mbr')"
  [ -n "${LEVELS}" ] && {
    for LEVEL in ${LEVELS}
     do
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")}"
      break
    done
    echo "## Create mbr partition(s) via fdisk, e.g. as partition ${DEVNO}"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      echo "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${DEV}' ; # ${DEV}${DEVNO} with all available space"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
    done
  }
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'gpt')"
  [ -n "${LEVELS}" ] && {
    for LEVEL in ${LEVELS}
     do
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")}"
      break
    done
    echo "## Create gpt partition(s) via gdisk, e.g. as partition ${DEVNO}"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      echo "gdisk '${DEV}' ; # ${DEV}${DEVNO} with all available space"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
    done
  }
  echo '## partprobe or reboot to make new partition(s) known to kernel'
  echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
  echo '#'
  echo 'for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
  echo '## Overwrite all unencrypted data that may remain on the mbr/gpt partition(s)'
  for DEV in ${DEVS}
   do
    echo "dd if=/dev/urandom of='${DEV}' bs=1024k ; \\"
  done
  echo "printf '\\a' ; # TAKES TIME!!!"

  # partitions
  unset DEVS
  unset SUBDEVS
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'mbr')"
  [ -n "${LEVELS}" ] && {
    echo "## Delete mbr partition(s) via fdisk"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      echo "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${DEV}' ; # ${DEV}${DEVNO} delete"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
    done
  }
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'gpt')"
  [ -n "${LEVELS}" ] && {
    echo "## Delete gpt partition(s) via gdisk"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      echo "gdisk '${DEV}' ; # ${DEV}${DEVNO} delete"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
    done
  }
  echo '## partprobe or reboot to make new partition table(s) known to kernel'
  echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
  echo '#'
  echo 'for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
  echo ''

  return 0
}

show_newswap_suggestions () {
### TODO: analog show_newroot (although only for outdated non LVM setup)
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Wanted encryption key, e.g. none, /dev/[u]random, etc.
  # 3 = Wanted MDADM/Device Number, e.g. 6
  # 4 = Wanted Device Number, e.g. 6

  local BASENAME
  local BASEKEY
  local BASEMDADM
  local BASEDEVNO

  local LEVELS
  local LEVEL
  local PARTTYPE
  local DEV
  local DEVS
  local DEVNO
  local SUBDEVS
  local MDADM
  local MDADMCOUNT

  local TYPE
  local NEWSWAPCRYPTKEY
  local NEWSWAPCRYPTSCRIPT
  local NEWSWAPCRYPTPWOPTS
  local NEWSWAPCRYPTKEYOPTS
  local LUKSCIPHER

  [ -z "${2}" ] && {
    echo "Usage: show_newswap_suggestions <var name> <encryption key>" 1>&2
    return 1 2>/dev/null || exit 1
  }

  BASENAME="${1}"
  BASEKEY="${2}"
  BASEMDADM="${3}"
  BASEDEVNO="${4:-${BASEMDADM}}"

  unset MDADM

  unset TYPE
  unset NEWSWAPCRYPTKEY
  unset NEWSWAPCRYPTSCRIPT
  unset NEWSWAPCRYPTPWOPTS
  unset NEWSWAPCRYPTKEYOPTS

  NEWSWAPCRYPTKEY="${BASEKEY:-none}"
  case "${NEWSWAPCRYPTKEY}" in
    ('none')
      TYPE='luks'
      NEWSWAPCRYPTPWOPTS='--verify-passphrase --hash=sha512'
      ;;
    (/*)
      TYPE='swap'
      ;;
    (*)
      TYPE='luks'
      NEWSWAPCRYPTSCRIPT='/lib/cryptsetup/scripts/decrypt_derived'
      NEWSWAPCRYPTKEYOPTS='--key-file=-'
      ;;
  esac
  echo "swap type is ${TYPE}"

  # check for raid
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'mdadm')"
  if [ -n "${LEVELS}" ] ; then
    PARTTYPE='FD'
  else
    PARTTYPE='82'
  fi

  # file system
  echo ''
  echo "### Create device(s) for new encrypted swap"

  # partitions
  unset DEVS
  unset SUBDEVS
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'mbr')"
  [ -n "${LEVELS}" ] && {
    for LEVEL in ${LEVELS}
     do
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")}"
      break
    done
    echo "## Create mbr partition(s) for new root via fdisk, e.g. as partition ${DEVNO}"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      echo "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} ${DEV} ; # ${DEV}${DEVNO} with all available space and type ${PARTTYPE}"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
    done
  }
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'gpt')"
  [ -n "${LEVELS}" ] && {
    for LEVEL in ${LEVELS}
     do
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")}"
      break
    done
    echo "## Create gpt partition(s) for new root via gdisk, e.g. as partition ${DEVNO}"
    for LEVEL in ${LEVELS}
     do
      DEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      echo "gdisk ${DEV} ; # ${DEV}${DEVNO} with all available space and type ${PARTTYPE}00"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
    done
  }
  echo '## partprobe or reboot to make new partition(s) known to kernel'
  echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
  echo '#'
  echo 'for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
  echo '## Overwrite all unencrypted data that may remain on the mbr/gpt partition(s)'
  for DEV in ${DEVS}
   do
    echo "dd if=/dev/urandom of=${DEV} bs=1024k ; \\"
  done
  echo "printf '\\a' ; # TAKES TIME!!!"

  # mdadm
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'mdadm')"
  MDADMCOUNT=0
  for DEV in ${DEVS}
   do
    MDADMCOUNT="$(( ${MDADMCOUNT} + 1 ))"
  done
  LEVEL='raid1'
  DEVNO="${BASEMDADM:-0}"
  [ -n "${LEVELS}" ] && {
    for LEVEL in ${LEVELS}
     do
      [ -z "${BASEMDADM}" ] && {
        DEVNO="$(eval echo "\"\${${BASENAME}${LEVEL}}\"")"
        DEVNO="${DEVNO##*/}"
        DEVNO="$(echo "${DEVNO}" | sed -n -e 's#[^0-9]*\(.*\)$#\1#p')"
      }
      break
    done
  }
  DEV="/dev/md${DEVNO}"
  echo "## Combine partitions for new root to an mdadm raid, e.g. ${DEV}"
  [ -n "${LEVEL}" ] && {
    echo "# take options from mdadm --detail $(eval echo "\"\${${BASENAME}${LEVEL}}\"") BEFORE removing ${BASENAME}"
  }
  echo "mdadm --zero-superblock ${DEVS}"
  echo "mdadm --create ${DEV} -l $(eval echo "\"\${${BASENAME}${LEVEL}RLEVEL}\"") -n ${MDADMCOUNT} $([ "${DEBVERSION}" -ge 7 ] && echo '-b internal ')<more options> ${DEVS}"
  echo '## Rebuild /etc/mdadm/mdadm.conf'
  echo '/usr/share/mdadm/mkconf >/etc/mdadm/mdadm.conf.new'
  echo 'diff -s -u /etc/mdadm/mdadm.conf /etc/mdadm/mdadm.conf.new'
  echo 'mv /etc/mdadm/mdadm.conf.new /etc/mdadm/mdadm.conf'
  DEVS="/dev/md${DEVNO}"
  MDADM="${DEVS##*/}"

  # luks
  DEVNO="${MDADM}_crypt"
  #
  if [ "${TYPE}" = 'swap' ] ; then
    echo '## Set a new UUID for the device'
    echo "mkswap ${DEVS} ; printf '\\a'"
    echo '## Next create an encrypted device for new root'
    echo "blkid ${DEVS} ; sync ; # make sure that superblock is created and written"
  else
    if [ "${DEBVERSION}" -le 5 ] ; then
      LUKSCIPHER='--cipher=aes-cbc-essiv:sha256 --key-size=256'
    else
      LUKSCIPHER='--cipher=aes-xts-plain64 --key-size=512'
    fi
    echo '## Next create an encrypted device for new root'
    if [ -n "${NEWSWAPCRYPTSCRIPT}" ] ; then
      echo "${NEWSWAPCRYPTSCRIPT} ${NEWSWAPCRYPTKEY} | cryptsetup --verbose ${NEWSWAPCRYPTKEYOPTS:+${NEWSWAPCRYPTKEYOPTS} }${LUKSCIPHER} luksFormat ${DEVS}"
    else
      echo "cryptsetup --verbose ${NEWSWAPCRYPTPWOPTS:+${NEWSWAPCRYPTPWOPTS} }${LUKSCIPHER} luksFormat ${DEVS}"
    fi
  fi
  #
  echo '# update /etc/crypttab'
  if [ "${DEBVERSION}" -le 5 ] ; then
    echo "echo \"${DEVNO} ${DEVS} ${NEWSWAPCRYPTKEY} ${TYPE}${NEWSWAPCRYPTSCRIPT:+,keyscript=${NEWSWAPCRYPTSCRIPT}}$([ "${TYPE}" = 'swap' ] && echo ",offset=8")\" >>/etc/crypttab"
    echo 'nano /etc/crypttab'
    echo '#'
  else
    echo "echo \"${DEVNO} UUID=\$(cryptsetup luksUUID ${DEVS}) ${NEWSWAPCRYPTKEY} ${TYPE}${NEWSWAPCRYPTSCRIPT:+,keyscript=${NEWSWAPCRYPTSCRIPT}}$([ "${TYPE}" = 'swap' ] && echo ",offset=8")\" >>/etc/crypttab"
    echo 'nano /etc/crypttab'
    echo '#'
    [ "${DEBVERSION}" -le 6 ] && {
      echo "partprobe || blockdev --rereadpt ${DEVS} ; # otherwise reboot"
      echo '#'
      echo 'for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
    }
  fi
  if [ "${DEBVERSION}" -le 4 ] ; then
    echo "cryptsetup luksOpen ${DEVS} ${DEVNO}"
  else
    echo "cryptdisks_start ${DEVNO} || cryptsetup luksOpen ${DEVS} ${DEVNO}"
  fi
  DEVS="/dev/mapper/${DEVNO}"

  # file system
  if [ "${TYPE}" = 'luks' ] ; then
    echo '## Create file system for new swap'
    echo "mkswap ${DEVS} ; printf '\\a'"
  fi
  [ -n "${MDADM}" ] && {
    echo "mdadm_wait /dev/${MDADM} ; printf '\\a' ; # TAKES TIME!!!"
  }

  # script
  echo '## Set variable for new swap'
  echo "NEWSWAP='${DEVS}'"
  echo 'determine_subdevs NEWSWAP ${NEWSWAP} > /root/bin/luks_NEWSWAP.inc'
  echo 'cat >> /root/bin/luks_NEWSWAP.inc << __EOF'
  echo '#'
  echo "NEWSWAPCRYPTKEY='${NEWSWAPCRYPTKEY}'"
  echo "NEWSWAPCRYPTKEYOPTS='${NEWSWAPCRYPTKEYOPTS}'"
  echo "NEWSWAPCRYPTPWOPTS='${NEWSWAPCRYPTPWOPTS}'"
  echo "NEWSWAPCRYPTSCRIPT='${NEWSWAPCRYPTSCRIPT}'"
  echo '__EOF'
  echo ''

  return 0
}

show_luks () {
  set | grep -e '^DEB' -e '^OLD' -e '^GRUB' -e '^FDISK' -e '^MINI' -e '^TEMP' -e '^NEW'

  return 0
}
