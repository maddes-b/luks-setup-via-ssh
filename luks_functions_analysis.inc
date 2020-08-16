#!/bin/false

echo 'Setting analysis functions...' 1>&2

. /root/bin/custom_functions.inc 2>/dev/null

EXTRASIZEBASE='5' ; ## minimum reserve for device administration data
EXTRASIZEMDADM="$(( ${EXTRASIZEBASE} + 0 ))"
EXTRASIZELUKS="$(( ${EXTRASIZEBASE} + 0 ))"
EXTRASIZELVM="$(( ${EXTRASIZEBASE} + 5 ))" ; ## additional space for pvresize to match extents
EXTRASIZEFALLBACKPCT='10'


###
### Main Functions
###
print_subdevs_value () {
  ## Parameters:
  ## 1 = Base Name of Variable, e.g. OLDROOT
  ## 2 = Suffix of Target Value Variable, e.g. SUBDEVS
  ## 3 = Requested Types, e.g. mbr|gpt

  if [ -z "${1:-}" ]; then
    echo "Usage: print_subdevs_value <var name> <val name suffix> [<device type>[|<device type>[|...]]]" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  local RESULT

  unset -v RESULT
  get_subdevs_value "${1}" "${2:-}" "${3:-}" ''
  echo "${RESULT:-}"

  return 0
}

determine_subdevs () {
  ## Parameters:
  ## 1 = Base Name of Variable, e.g. OLDROOT
  ## 2 = Device to Investigate
  ## 3 = Minimal Size for File System in MiB
  ## 4 = Set mapper name as mount for file system

  local BASENAME
  local TESTDEV
  local MINSIZE
  local SETMAPPERMOUNT

  local DMNAME
  local MOUNTDEV
  local EXTRASIZE

  ## for eval_blkid()
  local UUID
  local UUID_SUB
  local LABEL
  local TYPE

  unset -v DMNAME MOUNTDEV
  unset -v EXTRASIZE
  unset -v UUID UUID_SUB LABEL TYPE ; ## for eval_blkid()

  if [ -z "${1:-}" ]; then
    echo "Usage: determine_subdevs <var name> [<device> [<min size> [<set mapper mount>]]]" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  BASENAME="${1}"
  TESTDEV="${2:-}"
  MINSIZE="${3:-}"
  SETMAPPERMOUNT="${4:-}"

  echo "echo 'Setting ${BASENAME} vars...' 1>&2"

  if [ -z "${TESTDEV}" ]; then
    echo "unset -v ${BASENAME}MOUNT"
    echo "unset -v ${BASENAME}"
    return 0
  fi

  if [ ! -e "${TESTDEV}" ]; then
    echo "device ${TESTDEV} does NOT exist" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  blkid "${TESTDEV}" >/dev/null ; sync ; ## make sure that super block is created and written

  get_dmname "${TESTDEV}"

  echo "## All sizes are in MiB (2^20 bytes)"

  ## Variable that holds the name under which the file system is mounted.
  ## When a mapper name is available, then Debian recommends to use it for mounting.
  ## For a root file system on LVM this is even mandatory in /etc/fstab for booting to work.
  ## On Debian 4.0 this is even mandatory in /etc/fstab for cryptroot to work.
  [ -n "${SETMAPPERMOUNT}" -a -n "${DMNAME}" ] && MOUNTDEV="/dev/mapper/${DMNAME}"
  echo "${BASENAME}MOUNT='${MOUNTDEV:-${TESTDEV}}'"

  ## Variable that holds the best name for the file system, no matter what.
  ## Up to Debian 4.0 some commands cannot use a mapper name as a device, e.g. lvm
  [ -z "${DMNAME}" ] || TESTDEV="/dev/mapper/${DMNAME}"

  eval_blkid "${TESTDEV}"
  case "${TESTDEV}" in
    (*by-uuid*) TESTDEV="$(findfs UUID=${UUID})" ;;
    (*by-label*) TESTDEV="$(findfs LABEL=${LABEL})" ;;
    (*) ;;
  esac

  echo "${BASENAME}='${TESTDEV}'"
  echo "${BASENAME}TYPE='${TYPE}'"
  echo "${BASENAME}UUID='${UUID}'"
  [ -z "${UUID_SUB}" ] || echo "${BASENAME}UUID_SUB='${UUID_SUB}'"
  echo "${BASENAME}LABEL='${LABEL}'"
  if [ -z "${MINSIZE}" ]; then
    echo "unset -v ${BASENAME}SIZEMIN ; ## ='x'"
  else
    echo "${BASENAME}SIZEMIN='${MINSIZE}'"
  fi

  echo "${BASENAME} via ${TYPE} on ${TESTDEV}${MINSIZE:+ and ${TYPE} min size of ${MINSIZE} MiB}" 1>&2

  EXTRASIZE="$(get_extrasize_device_type "${TYPE}")"
  get_subdevs "${BASENAME}" '' "${TESTDEV}" "${MINSIZE}" "${EXTRASIZE}"

  return 0
}


###
### Sub Functions
###
get_subdevs_value () {
  ## Parameters:
  ## 1 = Base Name of Variable, e.g. OLDROOT
  ## 2 = Suffix of Target Value Variable, e.g. SUBDEVS
  ## 3 = Requested Types, e.g. mbr|gpt
  ## 4 = Level, e.g. _1

  local BASENAME
  local VALNAMES
  local TYPES
  local LEVEL

  local TYPE
  local FOUND
  local VALNAME
  local SUBLEVEL

  BASENAME="${1:-}"
  VALNAMES="${2:-DEV}"
  TYPES="${3:-mbr|gpt}"
  LEVEL="${4:-}"

  if [ -n "${LEVEL}" ]; then
    TYPE="$(eval echo "\"\${${BASENAME}${LEVEL}TYPE}\"")"
    FOUND=0
## eval hack, due to shells not being able to handle CASE ORs (=pipe symbol) in variables
    eval "case \"${TYPE}\" in
            (${TYPES}) FOUND=1 ;;
          esac"
    if [ "${FOUND}" -ne 0 ]; then
      for VALNAME in ${VALNAMES}
       do
        [ "${VALNAME}" != 'DEV' ] || VALNAME=''
        if [ "${VALNAME}" = 'LEVEL' ]; then
          RESULT="${RESULT:+${RESULT} }${LEVEL}"
        else
          RESULT="${RESULT:+${RESULT} }$(eval echo "\"\${${BASENAME}${LEVEL}${VALNAME}}\"")"
        fi
      done
    fi
  fi

  for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
   do
    get_subdevs_value "${BASENAME}" "${VALNAMES}" "${TYPES}" "${SUBLEVEL}"
  done

  return 0
}

get_extrasize_device_type () {
  ## Parameters:
  ## 1 = Device Type

  local TYPE

  local EXTRASIZE

  TYPE="${1}"

  EXTRASIZE=-1

  case "${TYPE}" in
   (ext4|ext3|ext2) EXTRASIZE="( \${BASESIZE} * 10 / 100 ) + ${EXTRASIZEBASE}" ; ## 10% is a good value for the administration overhead and the typical 5% reserved blocks of ext file systems
          ;;
   (swap) EXTRASIZE="${EXTRASIZEBASE}"
          ;;
   (lvm) EXTRASIZE="${EXTRASIZELVM}"
          ;;
   (luks) EXTRASIZE="${EXTRASIZELUKS}"
          ;;
   (mdadm) EXTRASIZE="${EXTRASIZEMDADM}"
          ;;
   (*) printf -- '\a' 1>&2
       echo "ERROR!!! Extra size calculation for type ${TYPE} missing!" 1>&2
       echo "Please maintain this type in get_extrasize_device_type()." 1>&2
       echo "Falling back to ${EXTRASIZEFALLBACKPCT}% + ${EXTRASIZEBASE}M" 1>&2
       EXTRASIZE="( \${BASESIZE} * ${EXTRASIZEFALLBACKPCT} / 100 ) + ${EXTRASIZEBASE}"
       printf -- '\a' 1>&2
       ;;
  esac

  echo "${EXTRASIZE}"

  return 0
}

get_subdevs () {
  ## Parameters:
  ## 1 = Base Name of Variable, e.g. OLDROOT
  ## 2 = Level Prefix of Caller (OLDPREFIX)
  ## 3 = Device List
  ## 4 = Full Size of Caller
  ## 5 = Extra Size to be added on these Devices

  local BASENAME
  local OLDPREFIX
  local DEVLIST
  local MINSIZE
  local SIZE

  local LEVEL
  local TESTDEV
  local EXTRADEV

  local PREFIX
  local SUBLEVELS
  local NEWDEVLIST
  local DMNAME
  local DEV
  local DEVTYPE
  local EXTRASIZE
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

  ## for eval_blkid()
  local UUID
  local UUID_SUB
  local LABEL
  local TYPE

  BASENAME="${1}"
  OLDPREFIX="${2}"
  DEVLIST="${3}"
  MINSIZE="${4}"
  SIZE="${5}"
  [ -z "${DEVLIST}" ] && return 0

  echo "${OLDPREFIX}_*: ${DEVLIST}" 1>&2

  unset -v PREFIX
  unset -v SUBLEVELS
  unset -v NEWDEVLIST
  unset -v EXTRASIZE
  unset -v DMNAME DEV DEVTYPE
  unset -v LVM2_LV_UUID LVM2_LV_NAME LVM2_VG_NAME
  unset -v MD_LEVEL MD_UUID MD_NAME MD_METADATA MD_DATASTART
  unset -v MAINDEV
  unset -v UUID UUID_SUB LABEL TYPE ; ## for eval_blkid()

  [ -n "${SIZE}" ] && {
    [ -n "${MINSIZE}" ] && {
      eval "MINSIZE=\$(( \${MINSIZE} + $(echo "${SIZE}" | sed -e 's#${BASESIZE}#${MINSIZE}#') ))"
    }
    SIZE="$(echo "${SIZE}" | sed -e "s#\${BASESIZE}#\${${BASENAME}${OLDPREFIX}SIZEMIN}#")"
  }

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

    ## LVM
    unset -v EXTRADEV
    [ -n "${DMNAME}" -a "${DEBVERSION}" -le 4 ] && {
      echo "${TESTDEV}" | grep -q -F -e '-' && {
        EXTRADEV="${TESTDEV}"
        TESTDEV="/dev/$(echo "${DMNAME}" | sed -n -e 's#\([^-]\)-\([^-]\)#\1/\2# ; s#--#-# ; p ;')"
      }
    }
    lvm lvs --noheadings -o LV_NAME "${TESTDEV}" 1>/dev/null 2>&1 && {
      DEVTYPE='lvm'
      echo "${BASENAME}${PREFIX}='${TESTDEV}'"
      echo "${BASENAME}${PREFIX}TYPE='${DEVTYPE}'"
      eval_lvm "${TESTDEV}"
      echo "${BASENAME}${PREFIX}UUID='${LVM2_LV_UUID}'"
#     echo "${BASENAME}${PREFIX}LABEL='...'"
      echo "${BASENAME}${PREFIX}DMNAME='${DMNAME}'"
      echo "${BASENAME}${PREFIX}LV='${LVM2_LV_NAME}'"
      echo "${BASENAME}${PREFIX}VG='${LVM2_VG_NAME}'"
#     echo "${BASENAME}${PREFIX}VG_UUID='...vgs/vgdisplay...'"
#     echo "${BASENAME}${PREFIX}PV_UUID='...pvs/pvdisplay...'"
      unset -v NEWDEVLIST
      for DEV in ${LVM2_DEVICES}
       do
        get_dmname "${DEV}"
        [ -n "${DMNAME}" ] && DEV="/dev/mapper/${DMNAME}"
        NEWDEVLIST="${NEWDEVLIST:+${NEWDEVLIST} }${DEV}"
      done
      echo "${BASENAME}${PREFIX}SUBDEVS='${NEWDEVLIST}'"
      if [ -z "${MINSIZE}" ]; then
        echo -n "unset -v ${BASENAME}${PREFIX}SIZEMIN ; ## "
      else
        echo -n "${BASENAME}${PREFIX}SIZEMIN"
      fi
      echo "=\"\$(( \${${BASENAME}${OLDPREFIX}SIZEMIN} + ${SIZE:-?} ))\""
      echo "## ATTENTION! Sizes for lvm physical devices below are only based on the above"
      echo "##            single logical volume instead of the actual pv usage. Please"
      echo "##            maintain the necessary pv sizes in ${BASENAME}${PREFIX}_?SIZEMIN"
      echo "##            with an extra ${EXTRASIZELVM} MiB buffer for each pv."
      echo "${TESTDEV} via ${DEVTYPE} on ${NEWDEVLIST}${MINSIZE:+ with lv min size of ${MINSIZE} MiB}" 1>&2
      EXTRASIZE="$(get_extrasize_device_type "${DEVTYPE}")"
      get_subdevs "${BASENAME}" "${PREFIX}" "${NEWDEVLIST}" "${MINSIZE}" "${EXTRASIZE}"
      continue
    }
    [ -n "${EXTRADEV}" ] && TESTDEV="${EXTRADEV}"
    unset -v EXTRADEV

    ## LUKS
    cryptsetup status "${TESTDEV}" 1>/dev/null 2>&1 && {
      DEVTYPE='luks'
      echo "${BASENAME}${PREFIX}='${TESTDEV}'"
      echo "${BASENAME}${PREFIX}TYPE='${DEVTYPE}'"
      NEWDEVLIST="$(cryptsetup status "${TESTDEV}" 2>/dev/null | sed -n -e 's#/dev/.static##' -e 's#^[[:space:]]\+device:[[:space:]]\+\([^[:space:]]\+\)[[:space:]]*#\1#p')"
      echo "${BASENAME}${PREFIX}UUID='$(cryptsetup luksUUID "${NEWDEVLIST}" 2>/dev/null)'"
#     echo "${BASENAME}${PREFIX}LABEL='...'"
      echo "${BASENAME}${PREFIX}DMNAME='${DMNAME}'"
      get_dmname "${NEWDEVLIST}"
      [ -n "${DMNAME}" ] && NEWDEVLIST="/dev/mapper/${DMNAME}"
      echo "${BASENAME}${PREFIX}SUBDEVS='${NEWDEVLIST}'"
      if [ -z "${MINSIZE}" ]; then
        echo -n "unset -v ${BASENAME}${PREFIX}SIZEMIN ; ## "
      else
        echo -n "${BASENAME}${PREFIX}SIZEMIN"
      fi
      echo "=\"\$(( \${${BASENAME}${OLDPREFIX}SIZEMIN} + ${SIZE:-?} ))\""
      echo "${TESTDEV} via ${DEVTYPE} on ${NEWDEVLIST}${MINSIZE:+ with ${DEVTYPE} min size of ${MINSIZE} MiB}" 1>&2
      EXTRASIZE="$(get_extrasize_device_type "${DEVTYPE}")"
      get_subdevs "${BASENAME}" "${PREFIX}" "${NEWDEVLIST}" "${MINSIZE}" "${EXTRASIZE}"
      continue
    }

    ## MD Software RAID
    mdadm --detail "${TESTDEV}" --brief 1>/dev/null 2>&1 && {
      DEVTYPE='mdadm'
      echo "${BASENAME}${PREFIX}='${TESTDEV}'"
      MAINDEV="${TESTDEV%%[0-9_]*}"
      MAINDEV="${MAINDEV%/}"
      if [ "${MAINDEV}/${TESTDEV##*[!0-9_]}" != "${TESTDEV}" ]; then
        echo "${BASENAME}${PREFIX}ALT='${MAINDEV}/${TESTDEV##*[!0-9_]}'"
      else
        echo "${BASENAME}${PREFIX}ALT='${MAINDEV}${TESTDEV##*[!0-9_]}'"
      fi
      unset -v MAINDEV
      echo "${BASENAME}${PREFIX}TYPE='${DEVTYPE}'"
      eval_mdadm "${TESTDEV}"
      echo "${BASENAME}${PREFIX}UUID='${MD_UUID}'"
      echo "${BASENAME}${PREFIX}LABEL='${MD_NAME}'"
      echo "${BASENAME}${PREFIX}METADATA='${MD_METADATA}'"
      MD_DATASTART="$(${CALL_ENV} mdadm --examine ${MD_DEVICES} | sed -n -e 's#^[[:space:]]*Data Offset : \([^[:space:]]\+\) sectors#\1#p' | sort -n -r | head -n 1)"
      if [ -n "${MD_DATASTART}" -a "${MD_DATASTART:-0}" -ne 0 ]; then
        MD_DATASTART="$(( ${MD_DATASTART} / ( 2 * 1024 ) ))"
        echo "${BASENAME}${PREFIX}DATASTART='${MD_DATASTART}'"
      else
        unset -v MD_DATASTART
        echo "unset -v ${BASENAME}${PREFIX}DATASTART"
      fi
      echo "${BASENAME}${PREFIX}SUBDEVS='${MD_DEVICES}'"
#     echo "${BASENAME}${PREFIX}DMNAME='${DMNAME}'"
      echo "${BASENAME}${PREFIX}RLEVEL='${MD_LEVEL}'"
      if [ -z "${MINSIZE}" ]; then
        echo -n "unset -v ${BASENAME}${PREFIX}SIZEMIN ; ## "
      else
        echo -n "${BASENAME}${PREFIX}SIZEMIN"
      fi
      echo "=\"\$(( \${${BASENAME}${OLDPREFIX}SIZEMIN} + ${SIZE:-?} ))\""
      echo "${TESTDEV} via ${DEVTYPE} on ${MD_DEVICES}${MINSIZE:+ with ${DEVTYPE} min size of ${MINSIZE} MiB}" 1>&2
      EXTRASIZE="$(get_extrasize_device_type "${DEVTYPE}")${MD_DATASTART:+ + ${MD_DATASTART}}"
      get_subdevs "${BASENAME}" "${PREFIX}" "${MD_DEVICES}" "${MINSIZE}" "${EXTRASIZE}"
      continue
    }

    ## Disk Partitions (mbr/gpt; Rest is unknown)
    MAINDEV="$(echo "${TESTDEV}" | sed -n -e 's#\([^0-9]*\)\([0-9]*\)$#\1#p')"

    ## GPT via fdisk
    ${CALL_ENV} fdisk -l "${MAINDEV}" 2>&1 1>/dev/null | grep -q -F -e 'WARNING: GPT (GUID Partition Table)' && {
      DEVTYPE='gpt'
      echo "${BASENAME}${PREFIX}='${TESTDEV}'"
      echo "${BASENAME}${PREFIX}TYPE='${DEVTYPE}'"
      eval_blkid "${TESTDEV}"
      echo "${BASENAME}${PREFIX}UUID='${UUID}'"
      [ -n "${UUID_SUB}" ] && echo "${BASENAME}${PREFIX}UUID_SUB='${UUID_SUB}'"
      echo "${BASENAME}${PREFIX}LABEL='${LABEL}'"
      echo "${BASENAME}${PREFIX}SUBDEVS='${MAINDEV}'"
      echo "${BASENAME}${PREFIX}SUBPART='$(echo "${TESTDEV}" | sed -n -e 's#\([^0-9]*\)\([0-9]*\)$#\2#p')'"
      if [ -z "${MINSIZE}" ]; then
        echo -n "unset -v ${BASENAME}${PREFIX}SIZEMIN ; ## "
      else
        echo -n "${BASENAME}${PREFIX}SIZEMIN"
      fi
      echo "=\"\$(( \${${BASENAME}${OLDPREFIX}SIZEMIN} + ${SIZE:-?} ))\""
      echo "${TESTDEV} via ${DEVTYPE}${MINSIZE:+ with ${DEVTYPE} min size of ${MINSIZE} MiB}" 1>&2
      continue
    }

    ## MBR via fdisk
    ${CALL_ENV} fdisk -l "${MAINDEV}" 2>&1 1>/dev/null | grep -q -F -e "doesn't contain a valid partition table" || {
      DEVTYPE='mbr'
      echo "${BASENAME}${PREFIX}='${TESTDEV}'"
      echo "${BASENAME}${PREFIX}TYPE='${DEVTYPE}'"
      eval_blkid "${TESTDEV}"
      echo "${BASENAME}${PREFIX}UUID='${UUID}'"
      [ -n "${UUID_SUB}" ] && echo "${BASENAME}${PREFIX}UUID_SUB='${UUID_SUB}'"
      echo "${BASENAME}${PREFIX}LABEL='${LABEL}'"
      echo "${BASENAME}${PREFIX}SUBDEVS='${MAINDEV}'"
      echo "${BASENAME}${PREFIX}SUBPART='$(echo "${TESTDEV}" | sed -n -e 's#\([^0-9]*\)\([0-9]*\)$#\2#p')'"
      if [ -z "${MINSIZE}" ]; then
        echo -n "unset -v ${BASENAME}${PREFIX}SIZEMIN ; ## "
      else
        echo -n "${BASENAME}${PREFIX}SIZEMIN"
      fi
      echo "=\"\$(( \${${BASENAME}${OLDPREFIX}SIZEMIN} + ${SIZE:-?} ))\""
      echo "${TESTDEV} via ${DEVTYPE}${MINSIZE:+ with ${DEVTYPE} min size of ${MINSIZE} MiB}" 1>&2
      continue
    }

    ## unknown
    echo "${BASENAME}${PREFIX}='${TESTDEV}'"
    echo "${BASENAME}${PREFIX}TYPE='unknown'"
    echo "${TESTDEV} via unknown" 1>&2
    continue
  done ; ## for-loop

  [ -n "${SUBLEVELS}" ] && echo "${BASENAME}${OLDPREFIX}SUBLEVELS='${SUBLEVELS}'"

  return 0
}

show_temp_suggestions () {
  ## Parameters:
  ## 1 = Base Name of Old Variable, e.g. OLDROOT
  ## 2 = Base Name of New Variable, e.g. TEMPEX
  ## 3 = Wanted Device Number, e.g. 9
  ## 4 = Full Size of Caller (default is <OLDBASENAME>SIZEMIN)
  ##
  ## Internal Parameters for recursive calls:
  ## 5 = Extra Size to be added on these Devices
  ## 6 = Level Prefix
  ## 7 = Type of Caller

  ## Internal "Globals" are:
  ## * SUBDEV = Last SUBDEV
  ## * SUBSUBDEVS = SUBDEVS of SUBDEV
  ## * MDADM = All mdadm devs

  local OLDBASENAME
  local NEWBASENAME
  local BASEDEVNO
  local ORGMINSIZE
  local SIZE
  local LEVEL
  local CALLERTYPE

  local MINSIZE

  local TYPE

  local SUBLEVELS
  local SUBLEVEL
  local EXTRASIZE
  local SUBTYPE
  local SUBDEVS

  local DEVNO
  local DEV

  ## for luks
  local LUKSCIPHER

  ## for mdadm
  local MDADMCOUNT
  local RLEVEL

  ## for mbr/gpt
  local SUBTYPEDISK
  local SUBDEVSDISK
  local PARTTYPE
  local SUBSIZE
  local SECSIZE

  [ -z "${2}" ] && {
    echo "Usage: show_temp_suggestions <old var name> <new var name> [<part/mdadm device number> [<wanted size>]]" 1>&2
    return 1 2>/dev/null || exit 1
  }

  OLDBASENAME="${1}"
  NEWBASENAME="${2}"
  BASEDEVNO="${3}"
  ORGMINSIZE="${4}"
  SIZE="${5}"
  LEVEL="${6}"
  CALLERTYPE="${7}"

  [ -z "${LEVEL}" ] && {
    local SUBDEV
    local SUBSUBDEVS
    local MDADM
  }

  MINSIZE="${ORGMINSIZE:-0}"
  [ "${MINSIZE}" -eq 0 ] && MINSIZE="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SIZEMIN}\"")"
  MINSIZE="${MINSIZE:-0}"
  #
  [ -n "${SIZE}" ] && {
    [ -n "${MINSIZE}" ] && {
      eval "MINSIZE=\$(( \${MINSIZE} + $(echo "${SIZE}" | sed -e 's#${BASESIZE}#${MINSIZE}#') ))"
    }
  }

  [ -z "${LEVEL}" ] && {
    echo ''
    echo "### Create device(s) for temporary storage to hold data of ${MINSIZE} MiB"
    echo -n "### while keeping the exact same build structure of ${OLDBASENAME}"
    [ "${ORGMINSIZE:-0}" -eq 0 ] && echo -n ' to re-use identical sizes'
    echo ''
    echo '### providing the same level of reliability and security'
    echo ''
  }

  TYPE="$(eval echo "\"\${${OLDBASENAME}${LEVEL}TYPE}\"")"

  unset -v SUBDEVS SUBTYPEDISK SUBDEVSDISK
  SUBLEVELS="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBLEVELS}\"")"
  [ -n "${SUBLEVELS}" ] && {
    EXTRASIZE="$(get_extrasize_device_type "${TYPE}")"
    for SUBLEVEL in ${SUBLEVELS}
     do
      SUBTYPE="$(eval echo "\"\${${OLDBASENAME}${SUBLEVEL}TYPE}\"")"
      case "${SUBTYPE}" in
       (mbr|gpt) SUBDEVSDISK="${SUBDEVSDISK:+${SUBDEVSDISK} }$(eval echo "\"\${${OLDBASENAME}${SUBLEVEL}SUBDEVS}\"")"
                 SUBTYPEDISK='X'
                 ;;
      esac
      show_temp_suggestions "${OLDBASENAME}" "${NEWBASENAME}" "${BASEDEVNO}" "${MINSIZE}" "${EXTRASIZE}" "${SUBLEVEL}" "${TYPE}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${SUBDEV}"
    done
    [ -n "${SUBTYPEDISK}" ] && {
      echo '## Partprobe or reboot to make new partition(s) known to kernel'
      echo "partprobe || blockdev --rereadpt ${SUBDEVSDISK} ; ## otherwise reboot"
      echo ' for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
    }
  }

  [ "${TYPE}" = 'swap' ] && TYPE="${OLDROOTTYPE}"
  case "${TYPE}" in
   (swap) DEV="${SUBDEVS}"
          echo '## Create swap'
          echo "mkswap '${SUBDEVS}' ; printf -- '\\a'"
          [ -n "${MDADM}" ] && {
            echo "mdadm_wait ${MDADM} ; printf -- '\\a' ; ## TAKES TIME!!!"
          }
          ;;
   (ext4|ext3|ext2)
          DEV="${SUBDEVS}"
          echo '## Create file system'
          echo "mkfs.${TYPE} '${SUBDEVS}' ; printf -- '\\a'"
          [ -n "${MDADM}" ] && {
            echo "mdadm_wait ${MDADM} ; printf -- '\\a' ; ## TAKES TIME!!!"
          }
          ;;
   (lvm) VG="$(eval echo "\"\${${OLDBASENAME}${LEVEL}VG}\"")"
         [ -z "${VG}" ] && VG="${HOSTNAME}_${NEWBASENAME}"
         DEVNO="${NEWBASENAME}"
         DEV="/dev/${VG}/${DEVNO}"
         echo "## Add new physical volume to volume group"
         echo "lvm pvcreate ${SUBDEVS}"
         echo "lvm vgextend '${VG}' ${SUBDEVS} || lvm vgcreate '${VG}' ${SUBDEVS}"
         echo "lvm vgdisplay -v --units m '${VG}'"
         echo "lvm pvdisplay -v --units m"
         echo "## Add new logical volume"
         if [ "${DEBVERSION}" -le 4 ]; then
           echo "lvm lvcreate -n '${DEVNO}' -L ${MINSIZE}M '${VG}' ${SUBDEVS}"
         else
           echo "lvm lvcreate -n '${DEVNO}' -l 100%PVS '${VG}' ${SUBDEVS}"
         fi
         echo "lvm lvs -o +DEVICES --units m '${VG}'"
         if [ "${DEBVERSION}" -le 4 ]; then
           echo "lvm lvdisplay -v -m"
         fi
         echo "lvm pvdisplay -v -m ${SUBDEVS} ; ## or lvm pvs -v --units m --segments ${SUBDEVS}"
         ;;
   (luks) DEVNO="${SUBDEVS#/dev/}_crypt"
          DEV="/dev/mapper/${DEVNO}"
          if [ "${DEBVERSION}" -le 5 ]; then
            LUKSCIPHER='--cipher=aes-cbc-essiv:sha256 --key-size=256'
          else
            LUKSCIPHER='--cipher=aes-xts-plain64 --key-size=512'
          fi
          echo '## Create an encrypted device inside it'
          echo "cryptsetup --verbose --verify-passphrase --hash=sha512 ${LUKSCIPHER} luksFormat '${SUBDEVS}'"
          [ -n "${MDADM}" ] && {
            echo "mdadm_wait ${MDADM} ; printf -- '\\a' ; ## TAKES TIME!!!"
            echo '## Populate /dev/disk/by-uuid'
            echo "mdadm -S ${MDADM}"
            echo "mdadm -A ${MDADM}"
          }
          echo '## Update /etc/crypttab'
          if [ "${DEBVERSION}" -le 5 ]; then
            echo "echo \"$([ "${DEBVERSION}" -le 4 ] && echo '#')${DEVNO} ${SUBDEVS} none luks$([ "${DEBVERSION}" -ge 5 ] && echo ',noauto')\" >>/etc/crypttab"
            echo 'nano /etc/crypttab'
            echo '#'
          else
            echo "echo \"${DEVNO} UUID=\$(cryptsetup luksUUID ${SUBDEVS}) none luks,noauto\" >>/etc/crypttab"
            echo 'nano /etc/crypttab'
            echo '#'
            [ "${DEBVERSION}" -le 6 ] && {
              echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; ## otherwise reboot"
              echo ' for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
            }
          fi
          if [ "${DEBVERSION}" -le 4 ]; then
            echo "cryptsetup luksOpen '${SUBDEVS}' '${DEVNO}'"
          else
            echo "cryptdisks_start '${DEVNO}' || cryptsetup luksOpen '${SUBDEVS}' '${DEVNO}'"
          fi
          ;;
   (mdadm) MDADMCOUNT=0
           for DEV in ${SUBDEVS}
            do
             MDADMCOUNT="$(( ${MDADMCOUNT} + 1 ))"
           done
           RLEVEL="$(eval echo "\"\${${OLDBASENAME}${LEVEL}RLEVEL}\"")"
           [ -z "${RLEVEL}" ] && RLEVEL='raid1'
           DEVNO="${BASEDEVNO:-${DEVNO}}"
           DEV="/dev/md${DEVNO}"
           echo "## Combine partitions to an mdadm raid, e.g. ${DEV}"
           echo "mdadm --zero-superblock ${SUBDEVS}"
           echo "## Take options from mdadm --detail '$(eval echo "\"\${${OLDBASENAME}${LEVEL}}\"")'"
           echo "mdadm --create '${DEV}' -l ${RLEVEL} -n ${MDADMCOUNT} $([ "${DEBVERSION}" -ge 7 ] && echo '-b internal ')<more options> ${SUBDEVS}"
           echo '## Rebuild /etc/mdadm/mdadm.conf'
           echo '/usr/share/mdadm/mkconf >/etc/mdadm/mdadm.conf.new'
           echo 'diff -s -u /etc/mdadm/mdadm.conf /etc/mdadm/mdadm.conf.new'
           echo 'mv /etc/mdadm/mdadm.conf.new /etc/mdadm/mdadm.conf'
           echo 'update-initramfs -u -k all'
           echo '# if any initrd update fails, just create a new one from scratch'
           echo ' IVERSION="$(uname -r)" ; # FIXME: set to problematic initramfs version'
           echo ' rm -f /boot/initrd.img-${IVERSION} ; update-initramfs -c -k ${IVERSION}'
           echo '#'
           MDADM="${MDADM:+${MDADM} }${DEV}"
           ;;
   (gpt) DEVNO="${BASEDEVNO:-9}"
         SUBDEVS="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBDEVS}\"")"
         DEV="${SUBDEVS}${DEVNO}"
         if [ "${CALLERTYPE}" = 'mdadm' ]; then
           PARTTYPE='FD00'
         elif [ "${CALLERTYPE}" = 'lvm' ]; then
           PARTTYPE='8E00'
         elif [ "${CALLERTYPE}" = 'swap' ]; then
           PARTTYPE='8200'
         fi
         echo "## Create gpt partition via gdisk at the end of the device, e.g. as partition ${DEVNO}"
         echo -n "gdisk '${SUBDEVS}' ; ## ${DEV} with ${MINSIZE} MiB${PARTTYPE:+ and type ${PARTTYPE}}, e.g. start is -${MINSIZE}M"
         echo ", and for END sector use +${MINSIZE}M"
         ;;
   (mbr) DEVNO="${BASEDEVNO:-6}"
         SUBDEVS="$(eval echo "\"\${${OLDBASENAME}${LEVEL}SUBDEVS}\"")"
         DEV="${SUBDEVS}${DEVNO}"
         unset -v PARTTYPE
         if [ "${CALLERTYPE}" = 'mdadm' ]; then
           PARTTYPE='FD'
         elif [ "${CALLERTYPE}" = 'lvm' ]; then
           PARTTYPE='8E'
         elif [ "${CALLERTYPE}" = 'swap' ]; then
           PARTTYPE='82'
         fi
         echo "## Create mbr partition via fdisk at the end of the device, e.g. as partition ${DEVNO}"
         echo -n "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${SUBDEVS}' ; ## ${DEV} with ${MINSIZE} MiB${PARTTYPE:+ and type ${PARTTYPE}}, e.g. START sector is last aligned sector minus"
         for SECSIZE in 512 1024 2048 4096
          do
           SUBSIZE="$(( ( ${MINSIZE} * 1024 * 1024 ) / ${SECSIZE} ))"
           echo -n " ${SUBSIZE}s(${SECSIZE})"
         done
         echo -n ', and for END sector use '
         [ "${DEBVERSION}" -ge 6 ] && {
           SUBSIZE="$(( ${MINSIZE} * 1024 ))"
           echo -n "+${SUBSIZE}K or "
         }
         echo '+(sectors-1)'
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
         ;;
   (*) printf -- '\a' 1>&2
       echo "ERROR!!! Suggestions for type ${TYPE} missing!" 1>&2
       echo "Please maintain this type in show_temp_suggestions()." 1>&2
       printf -- '\a' 1>&2
       ;;
  esac

  [ -z "${LEVEL}" ] && {
    echo '## Set variables for temporary storage'
    echo "${NEWBASENAME}='${DEV}'"
    [ "${MINSIZE}" -ne 0 ] && echo "${NEWBASENAME}SIZEMIN='${MINSIZE}'"
    echo "${NEWBASENAME}_1='${SUBDEVS}'"
    echo "${NEWBASENAME}_1TYPE='${SUBTYPE}'"
    echo "${NEWBASENAME}_1SUBDEVS='${SUBSUBDEVS}'"
    echo ''
  }

  SUBDEV="${DEV}"
  SUBSUBDEVS="${SUBDEVS}"

  return 0
}

show_newroot_suggestions () {
  ## Parameters:
  ## 1 = Base Name of Variable, e.g. OLDROOT
  ## 2 = Wanted MDADM/Device Number, e.g. 6
  ## 3 = Wanted Device Number, e.g. 6
  ## 4 = Wanted encryption key, e.g. none, /dev/[u]random, etc.

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

  unset -v DEVS
  unset -v MDADM

  unset -v BOOTSIZE
  unset -v BOOTDEVS
  unset -v BOOTMDADM
  unset -v BOOTSUBTYPE
  unset -v BOOTSUBDEVS

  unset -v TYPE
  unset -v NEWSWAPCRYPTKEY
  unset -v NEWSWAPCRYPTSCRIPT
  unset -v NEWSWAPCRYPTPWOPTS
  unset -v NEWSWAPCRYPTKEYOPTS

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

  ## file system
  echo ''
  SIZE="$(eval echo "\"\${${OLDBASENAME}SIZEMIN}\"")"
  [ -z "${OLDBOOT}" ] && {
    BOOTSIZE='512'
    echo "### Create device(s) for new UNENCRYPTED BOOT to hold data of ${BOOTSIZE} MiB"
  }
  echo "### Create device(s) for new ENCRYPTED ROOT to hold data of ${SIZE} MiB"

  ## check for raid
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mdadm')"
  [ -n "${BASEMDADM}${LEVELS}" ] && {
    MDADM='yes'
    [ -z "${OLDBOOT}" ] && {
      BOOTMDADM='yes'
    }
  }

  ## partitions
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mbr|gpt')"
  [ -n "${LEVELS}" ] && {
    unset -v SUBDEVS
    unset -v DEVS
    unset -v BOOTSUBDEVS
    unset -v BOOTDEVS
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
        echo -n "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${DEV}' ; ## ${DEV}${DEVNO} with ${BOOTSIZE} MiB${BOOTMDADM:+ and type FD}, e.g. START sector is first aligned sector, and for END sector use "
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
        printf -- '\n'
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
      echo "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${DEV}' ; ## ${DEV}${DEVNO} with all available space${MDADM:+ and type FD}, but at least ${SIZE} MiB, e.g. "
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
      printf -- '\n'
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
        echo "gdisk '${DEV}' ; ## ${DEV}${DEVNO} with +${BOOTSIZE}M${BOOTMDADM:+ and type FD00}"
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
      echo "gdisk '${DEV}' ; ## ${DEV}${DEVNO} with all available space${MDADM:+ and type FD00}, but at least +${SIZE}M"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
    done
    SUBTYPE='gpt'
  }
  echo '## partprobe or reboot to make new partition(s) known to kernel'
  echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; ## otherwise reboot"
  echo ' for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
  echo '## Overwrite all unencrypted data that may remain on the mbr/gpt partition(s)'
  for DEV in ${BOOTDEVS} ${DEVS}
   do
    echo "dd if=/dev/urandom of='${DEV}' bs=1024k ; \\"
  done
  echo "printf -- '\\a' ; ## TAKES TIME!!!"

  ## mdadm
  LEVELS="$(print_subdevs_value "${OLDBASENAME}" 'LEVEL' 'mdadm')"
  [ -n "${BASEMDADM}${LEVELS}" ] && {
    MDADMCOUNT=0
    for DEV in ${DEVS}
     do
      MDADMCOUNT="$(( ${MDADMCOUNT} + 1 ))"
    done
    unset -v LEVEL
    unset -v RLEVEL
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
      echo "## a) for mbr disks"
      echo "sfdisk --dump ${SUBDEVS%% *} | sfdisk /dev/sdX"
      echo "## b)for gpt disks"
      echo "sgdisk --replicate=/dev/sdX ${SUBDEVS%% *}"
      echo "sgdisk --randomize-guids /dev/sdX"
      echo "## both) to add later to the array (IF SUPPORTED! NOT RECOMMENDED!)"
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
      echo "## take options from mdadm --detail '$(eval echo "\"\${${OLDBASENAME}${LEVEL}}\"")' BEFORE removing old root"
    }
    echo "mdadm --create '${DEV}' -l ${RLEVEL} -n $(if [ "${MDADMCOUNT:-0}" -le 1 ]; then echo 'Y'; else echo "${MDADMCOUNT}"; fi) $([ "${DEBVERSION}" -ge 7 ] && echo '-b internal ')<more options> ${DEVS}$([ "${MDADMCOUNT:-0}" -le 1 ] && echo ' /dev/sdXn')"
    echo '## Rebuild /etc/mdadm/mdadm.conf'
    echo '/usr/share/mdadm/mkconf >/etc/mdadm/mdadm.conf.new'
    echo 'diff -s -u /etc/mdadm/mdadm.conf /etc/mdadm/mdadm.conf.new'
    echo 'mv /etc/mdadm/mdadm.conf.new /etc/mdadm/mdadm.conf'
    echo 'update-initramfs -u -k all'
    echo '# if any initrd update fails, just create a new one from scratch'
    echo ' IVERSION="$(uname -r)" ; # FIXME: set to problematic initramfs version'
    echo ' rm -f /boot/initrd.img-${IVERSION} ; update-initramfs -c -k ${IVERSION}'
    echo '#'
    SUBDEVS="${DEVS}"
    SUBTYPE='mdadm'
    DEVS="${DEV}"
    MDADM="${DEVS}"
  }

  ## luks
  echo '## Create an encrypted device for new root'
  DEVNO="${DEVS#/dev/}_crypt"
  #
  if [ "${DEBVERSION}" -le 5 ]; then
    LUKSCIPHER='--cipher=aes-cbc-essiv:sha256 --key-size=256'
  else
    LUKSCIPHER='--cipher=aes-xts-plain64 --key-size=512'
  fi
  if [ -n "${NEWSWAPCRYPTSCRIPT}" ]; then
    echo "${NEWSWAPCRYPTSCRIPT} ${NEWSWAPCRYPTKEY} | cryptsetup --verbose ${NEWSWAPCRYPTKEYOPTS:+${NEWSWAPCRYPTKEYOPTS} }${LUKSCIPHER} luksFormat '${DEVS}'"
  else
    echo "cryptsetup --verbose ${NEWSWAPCRYPTPWOPTS:+${NEWSWAPCRYPTPWOPTS} }${LUKSCIPHER} luksFormat '${DEVS}'"
  fi
  [ -n "${MDADM}" ] && {
    echo "mdadm_wait '${MDADM}' ; printf -- '\\a' ; ## TAKES TIME!!!"
    echo '## Populate /dev/disk/by-uuid'
    echo "mdadm -S '${DEV}'"
    echo "mdadm -A '${DEV}'"
  }
  echo '## update /etc/crypttab'
  if [ "${DEBVERSION}" -le 5 ]; then
    echo "echo \"$([ "${DEBVERSION}" -le 4 ] && echo '#')${DEVNO} ${DEVS} ${NEWSWAPCRYPTKEY} ${TYPE}${NEWSWAPCRYPTSCRIPT:+,keyscript=${NEWSWAPCRYPTSCRIPT}}$([ "${TYPE}" = 'swap' ] && echo ",offset=8")$([ "${DEBVERSION}" -ge 5 ] && echo ',noauto')\" >>/etc/crypttab"
    echo 'nano /etc/crypttab'
    echo '#'
  else
    echo "echo \"${DEVNO} UUID=\$(cryptsetup luksUUID ${DEVS}) ${NEWSWAPCRYPTKEY} ${TYPE}${NEWSWAPCRYPTSCRIPT:+,keyscript=${NEWSWAPCRYPTSCRIPT}}$([ "${TYPE}" = 'swap' ] && echo ",offset=8"),noauto\" >>/etc/crypttab"
    echo 'nano /etc/crypttab'
    echo '#'
    [ "${DEBVERSION}" -le 6 ] && {
      echo "partprobe || blockdev --rereadpt ${DEVS} ; ## otherwise reboot"
      echo ' for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
    }
  fi
  if [ "${DEBVERSION}" -le 4 ]; then
    echo "cryptsetup luksOpen '${DEVS}' '${DEVNO}'"
  else
    echo "cryptdisks_start '${DEVNO}' || cryptsetup luksOpen '${DEVS}' '${DEVNO}'"
  fi
  SUBDEVS="${DEVS}"
  SUBTYPE='luks'
  DEVS="/dev/mapper/${DEVNO}"

  ## lvm
  echo '## Create/extend lvm group with encrypted device'
  echo '## NOTE: here the VG name could be switched/changed for the new encrypted root system'
  DEV="${HOSTNAME}"
  echo "lvm pvcreate ${DEVS}"
  echo "lvm vgcreate '${DEV}' ${DEVS} || lvm vgextend '${DEV}' ${DEVS}"
  echo "lvm vgdisplay -v --units m '${DEV}'"
  echo "lvm pvdisplay -v --units m"
  echo "## Add dummy logical volume for determine_subdevs"
  DEVNO="${NEWBASENAME}"
  echo "lvm lvcreate -n '${DEVNO}' -l 1 '${DEV}' ${DEVS}"
  SUBDEVS="${DEVS}"
  SUBTYPE='lvm'
  DEVS="/dev/${DEV}/${DEVNO}"

  ## file system
  [ -z "${OLDBOOT}" ] && {
    echo '## Create file system for new BOOT'
    echo "mkfs.ext2 ${BOOTDEVS} ; printf -- '\\a'"
  }
  echo '## Create file system for dummy new ROOT'
  echo "mkfs.$(eval echo "\"\${${OLDBASENAME}TYPE}\"") ${DEVS} ; printf -- '\\a'"

  ## sync
  echo '## Sync all devices'
  echo 'sync'
  [ -n "${MDADM}${BOOTMDADM}" ] && {
    echo "mdadm_wait ${BOOTMDADM:+${BOOTMDADM} }${MDADM} ; printf -- '\\a' ; ## TAKES TIME!!!"
  }

  ## script
  echo '## Set variables for new BOOT'
  if [ -z "${OLDBOOT}" ]; then
    echo "NEWBOOT='${BOOTDEVS}'"
    [ "${BOOTSIZE}" -ne 0 ] && echo "NEWBOOTSIZEMIN='${BOOTSIZE}'"
    echo "NEWBOOT_1TYPE='${BOOTSUBTYPE}'"
    echo "NEWBOOT_1SUBDEVS='${BOOTSUBDEVS}'"
  else
    echo "NEWBOOT='${OLDBOOT}'"
    [ -n "${OLDBOOTSIZEMIN}" ] && echo "NEWBOOTSIZEMIN='${OLDBOOTSIZEMIN}'"
    echo "NEWBOOT_1TYPE='${OLDBOOT_1TYPE}'"
    echo "NEWBOOT_1SUBDEVS='${OLDBOOT_1SUBDEVS}'"
  fi
  echo '## Set variables for new root'
  echo "${NEWBASENAME}='${DEVS}'"
  echo "${NEWBASENAME}SIZEMIN='$(eval echo "\"\${${OLDBASENAME}SIZEMIN}\"")'"
  echo "${NEWBASENAME}_1TYPE='${SUBTYPE}'"
  echo "${NEWBASENAME}_1SUBDEVS='${SUBDEVS}'"
  echo ''

  return 0
}

show_clear_suggestions () {
  ## Parameters:
  ## 1 = Base Name of Variable, e.g. TEMPEX
  ## 2 = Wanted Device Number, e.g. 6
  ## 3 = Input device (defaults to urandom, reasonable is also zero)

  local BASENAME
  local BASEDEVNO
  local FILLDEV

  local LEVELS
  local LEVEL
  local SUBDEV
  local DEVNO
  local DEV
  local DEVS
  local SUBDEVS

  [ -z "${1}" ] && {
    echo "Usage: show_clear_suggestions <var name> [<wanted device number>]" 1>&2
    return 1 2>/dev/null || exit 1
  }

  BASENAME="${1}"
  BASEDEVNO="${2}"
  FILLDEV="${3:-urandom}"

  echo ''
  echo "### Create temporary device(s) to overwrite unencrypted old data of free hdd space"
  echo ''

  ## partitions
  unset -v DEVS
  unset -v SUBDEVS
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'mbr')"
  [ -n "${LEVELS}" ] && {
    echo "## Create mbr partition(s) via fdisk${BASEDEVNO:+, e.g. as partition ${BASEDEVNO}}"
    for LEVEL in ${LEVELS}
     do
      SUBDEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")}"
      DEV="${SUBDEV}${DEVNO}"
      echo "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${SUBDEV}' ; ## ${DEV} with all available space"
      DEVS="${DEVS:+${DEVS} }${DEV}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${SUBDEV}"
    done
  }
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'gpt')"
  [ -n "${LEVELS}" ] && {
    echo "## Create gpt partition(s) via gdisk${BASEDEVNO:+, e.g. as partition ${BASEDEVNO}}"
    for LEVEL in ${LEVELS}
     do
      SUBDEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")}"
      DEV="${SUBDEV}${DEVNO}"
      echo "gdisk '${SUBDEV}' ; ## ${DEV} with all available space"
      DEVS="${DEVS:+${DEVS} }${DEV}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${SUBDEV}"
    done
  }
  echo '## partprobe or reboot to make new partition(s) known to kernel'
  echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; ## otherwise reboot"
  echo ' for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
  echo '## Overwrite all unencrypted data that may remain on the mbr/gpt partition(s)'
  for DEV in ${DEVS}
   do
    echo "dd if='/dev/${FILLDEV}' of='${DEV}' bs=1024k ; \\"
  done
  echo "printf -- '\\a' ; ## TAKES TIME!!!"

  ## partitions
  unset -v DEVS
  unset -v SUBDEVS
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'mbr')"
  [ -n "${LEVELS}" ] && {
    echo "## Delete mbr partition(s) via fdisk"
    for LEVEL in ${LEVELS}
     do
      SUBDEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")}"
      DEV="${SUBDEV}${DEVNO}"
      echo "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} '${SUBDEV}' ; ## ${DEV} delete"
      DEVS="${DEVS:+${DEVS} }${DEV}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${SUBDEV}"
    done
  }
  #
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'gpt')"
  [ -n "${LEVELS}" ] && {
    echo "## Delete gpt partition(s) via gdisk"
    for LEVEL in ${LEVELS}
     do
      SUBDEV="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
      DEVNO="${BASEDEVNO:-$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")}"
      DEV="${SUBDEV}${DEVNO}"
      echo "gdisk '${SUBDEV}' ; ## ${DEV} delete"
      DEVS="${DEVS:+${DEVS} }${DEV}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${SUBDEV}"
    done
  }
  echo '## partprobe or reboot to make new partition table(s) known to kernel'
  echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; ## otherwise reboot"
  echo ' for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
  echo ''

  return 0
}

show_newswap_suggestions () {
### TODO: analog show_newroot (although only for outdated non LVM setup)
  ## Parameters:
  ## 1 = Base Name of Variable, e.g. OLDROOT
  ## 2 = Wanted encryption key, e.g. none, /dev/[u]random, etc.
  ## 3 = Wanted MDADM/Device Number, e.g. 6
  ## 4 = Wanted Device Number, e.g. 6

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

  unset -v MDADM

  unset -v TYPE
  unset -v NEWSWAPCRYPTKEY
  unset -v NEWSWAPCRYPTSCRIPT
  unset -v NEWSWAPCRYPTPWOPTS
  unset -v NEWSWAPCRYPTKEYOPTS

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

  ## check for raid
  LEVELS="$(print_subdevs_value "${BASENAME}" 'LEVEL' 'mdadm')"
  if [ -n "${LEVELS}" ]; then
    PARTTYPE='FD'
  else
    PARTTYPE='82'
  fi

  ## file system
  echo ''
  echo "### Create device(s) for new encrypted swap"

  ## partitions
  unset -v DEVS
  unset -v SUBDEVS
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
      echo "fdisk -u${FDISKCPARM:+ ${FDISKCPARM}} ${DEV} ; ## ${DEV}${DEVNO} with all available space and type ${PARTTYPE}"
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
      echo "gdisk ${DEV} ; ## ${DEV}${DEVNO} with all available space and type ${PARTTYPE}00"
      DEVS="${DEVS:+${DEVS} }${DEV}${DEVNO}"
      SUBDEVS="${SUBDEVS:+${SUBDEVS} }${DEV}"
    done
  }
  echo '## partprobe or reboot to make new partition(s) known to kernel'
  echo "partprobe || blockdev --rereadpt ${SUBDEVS} ; ## otherwise reboot"
  echo ' for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
  echo '## Overwrite all unencrypted data that may remain on the mbr/gpt partition(s)'
  for DEV in ${DEVS}
   do
    echo "dd if=/dev/urandom of=${DEV} bs=1024k ; \\"
  done
  echo "printf -- '\\a' ; ## TAKES TIME!!!"

  ## mdadm
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
    echo "## take options from mdadm --detail $(eval echo "\"\${${BASENAME}${LEVEL}}\"") BEFORE removing ${BASENAME}"
  }
  echo "mdadm --zero-superblock ${DEVS}"
  echo "mdadm --create ${DEV} -l $(eval echo "\"\${${BASENAME}${LEVEL}RLEVEL}\"") -n ${MDADMCOUNT} $([ "${DEBVERSION}" -ge 7 ] && echo '-b internal ')<more options> ${DEVS}"
  echo '## Rebuild /etc/mdadm/mdadm.conf'
  echo '/usr/share/mdadm/mkconf >/etc/mdadm/mdadm.conf.new'
  echo 'diff -s -u /etc/mdadm/mdadm.conf /etc/mdadm/mdadm.conf.new'
  echo 'mv /etc/mdadm/mdadm.conf.new /etc/mdadm/mdadm.conf'
  echo 'update-initramfs -u -k all'
  echo '# if any initrd update fails, just create a new one from scratch'
  echo ' IVERSION="$(uname -r)" ; # FIXME: set to problematic initramfs version'
  echo ' rm -f /boot/initrd.img-${IVERSION} ; update-initramfs -c -k ${IVERSION}'
  echo '#'
  DEVS="/dev/md${DEVNO}"
  MDADM="${DEVS##*/}"

  ## luks
  DEVNO="${MDADM}_crypt"
  #
  if [ "${TYPE}" = 'swap' ]; then
    echo '## Set a new UUID for the device'
    echo "mkswap ${DEVS} ; printf -- '\\a'"
    echo '## Next create an encrypted device for new root'
    echo "blkid ${DEVS} ; sync ; ## make sure that superblock is created and written"
  else
    if [ "${DEBVERSION}" -le 5 ]; then
      LUKSCIPHER='--cipher=aes-cbc-essiv:sha256 --key-size=256'
    else
      LUKSCIPHER='--cipher=aes-xts-plain64 --key-size=512'
    fi
    echo '## Next create an encrypted device for new root'
    if [ -n "${NEWSWAPCRYPTSCRIPT}" ]; then
      echo "${NEWSWAPCRYPTSCRIPT} ${NEWSWAPCRYPTKEY} | cryptsetup --verbose ${NEWSWAPCRYPTKEYOPTS:+${NEWSWAPCRYPTKEYOPTS} }${LUKSCIPHER} luksFormat ${DEVS}"
    else
      echo "cryptsetup --verbose ${NEWSWAPCRYPTPWOPTS:+${NEWSWAPCRYPTPWOPTS} }${LUKSCIPHER} luksFormat ${DEVS}"
    fi
  fi
  #
  echo '# update /etc/crypttab'
  if [ "${DEBVERSION}" -le 5 ]; then
    echo "echo \"${DEVNO} ${DEVS} ${NEWSWAPCRYPTKEY} ${TYPE}${NEWSWAPCRYPTSCRIPT:+,keyscript=${NEWSWAPCRYPTSCRIPT}}$([ "${TYPE}" = 'swap' ] && echo ",offset=8")\" >>/etc/crypttab"
    echo 'nano /etc/crypttab'
    echo '#'
  else
    echo "echo \"${DEVNO} UUID=\$(cryptsetup luksUUID ${DEVS}) ${NEWSWAPCRYPTKEY} ${TYPE}${NEWSWAPCRYPTSCRIPT:+,keyscript=${NEWSWAPCRYPTSCRIPT}}$([ "${TYPE}" = 'swap' ] && echo ",offset=8")\" >>/etc/crypttab"
    echo 'nano /etc/crypttab'
    echo '#'
    [ "${DEBVERSION}" -le 6 ] && {
      echo "partprobe || blockdev --rereadpt ${DEVS} ; ## otherwise reboot"
      echo ' for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
    }
  fi
  if [ "${DEBVERSION}" -le 4 ]; then
    echo "cryptsetup luksOpen ${DEVS} ${DEVNO}"
  else
    echo "cryptdisks_start ${DEVNO} || cryptsetup luksOpen ${DEVS} ${DEVNO}"
  fi
  DEVS="/dev/mapper/${DEVNO}"

  ## file system
  if [ "${TYPE}" = 'luks' ]; then
    echo '## Create file system for new swap'
    echo "mkswap ${DEVS} ; printf -- '\\a'"
  fi
  [ -n "${MDADM}" ] && {
    echo "mdadm_wait /dev/${MDADM} ; printf -- '\\a' ; ## TAKES TIME!!!"
  }

  ## script
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
