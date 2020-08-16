#!/bin/false

echo 'Setting help functions...' 1>&2

. /root/bin/custom_functions.inc 2>/dev/null


###
### mdadm information:
### * metadata 0.9/1.0
###   ** due to being located at the end of the device, a device resize will loose the superblock,
###      therefore it must be remove, clear superblock, resize and add again for each device
### * metadata 1.1/1.2
###   ** bitmaps working since Debian 7.0
###   ** grow also updates device size since Debian 7.0
###   ** assemble option to update device since Debian 5.0
###   ** grow --size=max working since Debian 5.0


###
### Main Functions
###
start_fs_subdevs () {
  ## Direction: bottom up
  ## 1 = Base Name of Variable, e.g. OLDROOT
  ## 2 = Start Level, e.g. _1
  ## 3 = Level, e.g. _1

  local BASENAME
  local STARTLEVELS
  local LEVELS

  local LEVEL
  local SKIP
  local DEV
  local DEVALT
  local TYPE
  local DMNAME

  local SUBLEVEL

  if [ -z "${1:-}" ]; then
    echo "Usage: start_fs_subdevs <var name> [<start level prefix>]" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  BASENAME="${1}"
  STARTLEVELS="${2:-}"
  LEVELS="${3:-0}"

  for LEVEL in ${LEVELS}
   do
    [ "${LEVEL}" != '0' ] || LEVEL=''

    SKIP=0
    if [ -n "${STARTLEVELS}" ]; then
      SKIP=1
      for SUBLEVEL in ${STARTLEVELS}
       do
# eval hack, due to shells not being able to handle ORs (=pipe symbol) in variables
        eval "case \"${LEVEL:-0}\" in
               (${SUBLEVEL}*) SKIP=0
                              break
                              ;;
              esac"
      done
    fi

    for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
     do
      start_fs_subdevs "${BASENAME}" "${STARTLEVELS}" "${SUBLEVEL}" || { return 1 2>/dev/null || exit 1 ; }
    done

    DEV="$(eval echo "\"\${${BASENAME}${LEVEL}}\"")"
    TYPE="$(eval echo "\"\${${BASENAME}${LEVEL}TYPE}\"")"

    echo ">> START level ${LEVEL:-top}: ${DEV} via ${TYPE}"
    if [ "${SKIP}" -ne 0 ]; then
      echo '...skipping'
    else
      RC=0
      case "${TYPE}" in
       (ext2|ext3|ext4) ;;
       (lvm) lvm vgchange -a y "$(eval echo "\${${BASENAME}${LEVEL}VG}")" || { return 1 2>/dev/null || exit 1 ; }
             ;;
       (luks) DMNAME="$(eval echo "\"\${${BASENAME}${LEVEL}DMNAME}\"")"
              [ -e "/dev/mapper/${DMNAME}" ] || {
                if [ "${DEBVERSION}" -le 4 ]; then
                  cryptsetup luksOpen "$(eval echo "\${${BASENAME}${LEVEL}SUBDEVS}")" "${DMNAME}" || { return 1 2>/dev/null || exit 1 ; }
                else
                  cryptdisks_start "${DMNAME}" || cryptsetup luksOpen "$(eval echo "\${${BASENAME}${LEVEL}SUBDEVS}")" "${DMNAME}" || { return 1 2>/dev/null || exit 1 ; }
                fi
              }
              ;;
       (mdadm) mdadm --detail "${DEV}" --test --brief 1>/dev/null 2>&1 || RC="${?}"
               if [ "${RC}" -ge 2 ]; then
                 DEVALT="$(eval echo "\"\${${BASENAME}${LEVEL}ALT}\"")"
                 mdadm --assemble "${DEV}" || mdadm --assemble "${DEVALT}" || { return 1 2>/dev/null || exit 1 ; }
               fi
               mdadm_wait "${DEV}" ; # TAKES TIME!!!
               mdadm --detail "${DEV}"
               printf -- '\a'
               ;;
       (mbr|gpt) ;;
       (*) echo "> ATTENTION! Type \"${TYPE}\" of ${DEV} is not supported or unknown, therefore"
           echo '  it has to be handled manually/individually if any action is required.'
           printf -- '\a'
           return 1 2>/dev/null || exit 1
           ;;
      esac
    fi
  done

  return 0
}

stop_fs_subdevs () {
  # Direction: top down
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Stop Level, e.g. _1
  # 3 = Level, e.g. _1

  local BASENAME
  local STOPLEVELS
  local LEVELS

  local LEVEL
  local SKIP
  local DEV
  local TYPE
  local DMNAME

  local SUBLEVEL

  if [ -z "${1:-}" ]; then
    echo "Usage: stop_fs_subdevs <var name> [<stop level prefix>]" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  BASENAME="${1}"
  STOPLEVELS="${2:-}"
  LEVELS="${3:-0}"

  for LEVEL in ${LEVELS}
   do
    [ "${LEVEL}" != '0' ] || LEVEL=''

    SKIP=0
    if [ -n "${STOPLEVELS}" ]; then
      for SUBLEVEL in ${STOPLEVELS}
       do
# eval hack, due to shells not being able to handle ORs (=pipe symbol) in variables
        eval "case \"${LEVEL:-0}\" in
               (${SUBLEVEL}*) SKIP=1
                              break
                              ;;
              esac"
      done
    fi

    DEV="$(eval echo "\"\${${BASENAME}${LEVEL}}\"")"
    TYPE="$(eval echo "\"\${${BASENAME}${LEVEL}TYPE}\"")"

    echo ">> STOP level ${LEVEL:-top}: ${DEV} via ${TYPE}"
    if [ "${SKIP}" -ne 0 ]; then
      echo '...skipping'
    else
      case "${TYPE}" in
       (ext2|ext3|ext4) ;;
       (lvm) lvm vgchange -a n "$(eval echo "\${${BASENAME}${LEVEL}VG}")"
             ;;
       (luks) DMNAME="$(eval echo "\"\${${BASENAME}${LEVEL}DMNAME}\"")"
              if [ "${DEBVERSION}" -le 4 ]; then
                cryptsetup luksClose "${DMNAME}"
              else
                cryptdisks_stop "${DMNAME}"
              fi
              ;;
       (mdadm) mdadm --stop "${DEV}"
               ;;
       (mbr|gpt) ;;
       (*) echo "> ATTENTION! Type \"${TYPE}\" of ${DEV} is not supported or unknown, therefore"
           echo '  it has to be handled manually/individually if any action is required.'
           printf -- '\a'
           return 1 2>/dev/null || exit 1
           ;;
      esac
    fi

    for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
     do
      stop_fs_subdevs "${BASENAME}" "${STOPLEVELS}" "${SUBLEVEL}" || { return 1 2>/dev/null || exit 1 ; }
    done
  done

  return 0
}

check_fs_subdevs () {
  # Direction: top down
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Stop Level, e.g. _1
  # 3 = Level, e.g. _1

  local BASENAME
  local STOPLEVELS
  local LEVELS

  local LEVEL
  local SKIP
  local DEV
  local TYPE

  local SUBLEVEL

  if [ -z "${1:-}" ]; then
    echo "Usage: check_fs_subdevs <var name> [<stop level prefix>]" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  BASENAME="${1}"
  STOPLEVELS="${2:-}"
  LEVELS="${3:-0}"

  for LEVEL in ${LEVELS}
   do
    [ "${LEVEL}" != '0' ] || LEVEL=''

    SKIP=0
    if [ -n "${STOPLEVELS}" ]; then
      for SUBLEVEL in ${STOPLEVELS}
       do
# eval hack, due to shells not being able to handle ORs (=pipe symbol) in variables
        eval "case \"${LEVEL:-0}\" in
               (${SUBLEVEL}*) SKIP=1
                              break
                              ;;
              esac"
      done
    fi

    DEV="$(eval echo "\"\${${BASENAME}${LEVEL}}\"")"
    TYPE="$(eval echo "\"\${${BASENAME}${LEVEL}TYPE}\"")"

    echo ">> CHECK level ${LEVEL:-top}: ${DEV} via ${TYPE}"
    if [ "${SKIP}" -ne 0 ]; then
      echo '...skipping'
    else
      case "${TYPE}" in
       (ext2|ext3|ext4) fsck -f "${DEV}"
                        printf -- '\a'
                        ;;
       (lvm) lvm vgck
             if [ "${DEBVERSION}" -ge 5 ]; then
               lvm pvck "$(eval echo "\${${BASENAME}${LEVEL}SUBDEVS}")"
             fi
             echo '> ATTENTION! lvm is not supported, as its setup can be very complex (*),'
             echo '  therefore lvm volumes, groups and devs have to be handled manually/individually.'
             echo '  (*) multiple volumes in one group and/or multiple devs'
             echo '      with different sizes in one group.'
             printf -- '\a'
             ;;
       (luks) ;;
       (mdadm) echo '> Calling repair for mdadm raid device to detect defective sectors in advance.'
               echo '  Monitoring is possible in another session via "watch -- cat /proc/mdstat"'
               echo 'repair' > /sys/block/${DEV##*/}/md/sync_action
               mdadm_wait "${DEV}" ; # TAKES TIME!!!
               mdadm --detail "${DEV}"
               printf -- '\a'
               ;;
       (mbr|gpt) ;;
       (*) echo "> ATTENTION! Type \"${TYPE}\" of ${DEV} is not supported or unknown, therefore"
           echo '  it has to be handled manually/individually if any action is required.'
           printf -- '\a'
           ;;
      esac
    fi

    for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
     do
      check_fs_subdevs "${BASENAME}" "${STOPLEVELS}" "${SUBLEVEL}"
    done
  done

  return 0
}

shrink_fs_subdevs () {
  # Direction: top down; special case bottom up for mdadm partitions with metadata 1.1/1.2
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Start Level, e.g. _1
  # 3 = Restart Direction, e.g. [U]p or D[own], default [ ] for both
  # 4 = Level, e.g. _1
  # 5 = Upper Level, e.g. _1

  local BASENAME
  local STARTLEVELS
  local RESTART
  local LEVELS
  local UPPERLEVEL

  local LEVEL
  local SKIP
  local SKIPVALUE
  local SUBLEVEL
  local DEV
  local PART
  local TYPE
  local SUBTYPE
  local SIZE
  local SUBSIZE
  local BLOCKS
  local RC

  local LV
  local VG
  local SUBDEVS
  local SUBLEVELS

  if [ -z "${1:-}" ]; then
    echo "Usage: shrink_fs_subdevs <var name> [<start level prefix> [<restart direction> [<levels> [<upper level]]]" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  BASENAME="${1}"
  STARTLEVELS="${2:-}"
  RESTART="${3:-}"
  LEVELS="${4:-0}"
  UPPERLEVEL="${5:-}"

  for LEVEL in ${LEVELS}
   do
    [ "${LEVEL}" != '0' ] || LEVEL=''

    SKIP=0
    if [ -n "${STARTLEVELS}" ]; then
      if [ "${RESTART}" != 'U' ]; then
        SKIP=1
        SKIPVALUE=0
      else
        SKIPVALUE=1
      fi
      #
      for SUBLEVEL in ${STARTLEVELS}
       do
# eval hack, due to shells not being able to handle ORs (=pipe symbol) in variables
        eval "case \"${LEVEL:-0}\" in
               (${SUBLEVEL}*) SKIP="${SKIPVALUE}"
                              break
                              ;;
              esac"
      done
    fi

    DEV="$(eval echo "\"\${${BASENAME}${LEVEL}}\"")"
    TYPE="$(eval echo "\"\${${BASENAME}${LEVEL}TYPE}\"")"
    SIZE="$(eval echo "\"\${${BASENAME}${LEVEL}SIZEMIN}\"")"

    # Direction: top down
    if [ "${SKIP}" -eq 0 -a "${RESTART}" != 'U' ]; then
      echo ">> SHRINK level ${LEVEL:-top}: ${DEV} via ${TYPE} to ${SIZE} MiB"
      case "${TYPE}" in
       (ext2|ext3|ext4) SUBSIZE="$(${CALL_ENV} dumpe2fs -h "${DEV}" 2>/dev/null | sed -n -e 's#^Block size:[[:space:]]\+##p')"
                        BLOCKS="$(${CALL_ENV} resize2fs -P "${DEV}" 2>/dev/null | sed -n 's#^Estimated minimum size of the filesystem:[[:space:]]\+##p')"
                        SUBSIZE="$(( ${BLOCKS:-0} * ${SUBSIZE} / 1024 / 1024 + 1 ))"
                        if [ "${SUBSIZE}" -gt "${SIZE}" ]; then
                          echo "> Have to resize file system in two steps as current minimal size is ${SUBSIZE} MiB"
                          SUBSIZE="$(( ${SUBSIZE} + 100 ))"
                          echo "> First resize to ${SUBSIZE} MiB"
                          resize2fs "${DEV}" ${SUBSIZE}M || { return 1 2>/dev/null || exit 1 ; }
                          fsck -f "${DEV}"
                          echo "> Now resize to ${SIZE} MiB"
                        fi
                        resize2fs "${DEV}" ${SIZE}M || { return 1 2>/dev/null || exit 1 ; }
                        fsck -f "${DEV}"
                        ;;
       (lvm) LV="$(eval echo "\"\${${BASENAME}${LEVEL}LV}\"")"
             VG="$(eval echo "\"\${${BASENAME}${LEVEL}VG}\"")"
             SUBDEVS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
             SUBLEVELS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")"
             SUBSIZE="$(eval echo "\"\${${BASENAME}${LEVEL}_1SIZEMIN}\"")"
             echo "> ATTENTION! ${BASENAME} is installed on block devices that use lvm."
             echo "  LVM setups can be very complex(*) therefore volume groups and"
             echo "  physical volumes have to be handled manually/individually."
             echo "  (*) multiple volumes in one group and/or multiple devs"
             echo "      with different sizes in one group."
             echo "  Logical volume '${LV}' in volume group '${VG}' via '${SUBDEVS}'."
             echo "  The logical volume will be shrinked as requested."
             echo "  Use the freed space to defrag the physical volume(s)"
             echo "  via $(if [ "${DEBVERSION}" -le 4 ]; then echo 'lvdisplay'; else echo 'pvdisplay'; fi) -m and pvmove, which can also be done in pieces/chunks."
             echo "  If a physical volume is totally freed it can also be removed via vgreduce."
             echo "  After that derive the new necessary sizes of the physical volume(s)"
             echo "  and maintain these sizes in /root/bin/luks_${BASENAME}.inc"
             echo "  Reload definitions, resize physical volume(s) via pvresize and continue"
             echo "  with shrinking the sub devices."
             if [ "${DEBVERSION}" -le 4 ]; then
               echo "    lvm lvdisplay -v -m"
             fi
             echo "    lvm pvdisplay -v -m ${SUBDEVS} ; #or lvm pvs -v --units m --segments ${SUBDEVS}"
             echo "    lvm pvmove --alloc anywhere -n <lv> <dev>[:<oldstart>+<size>] <dev>:<newstart>+<size>"
             echo "    # optionally remove cleared pv from vg and pv itself"
             echo "    #lvm vgreduce '${VG}' <pv>"
             echo "    #lvm pvremove <pv>"
             echo "    #remove_fs_subdevs '${BASENAME}' '' <level of pv, see below pvresize>"
             echo "    vgs --units m -o vg_name,vg_size,vg_free '${VG}'"
             echo "    pvs --units m -o pv_name,pv_size,pv_free,pv_used ${SUBDEVS}"
             echo "    nano '/root/bin/luks_${BASENAME}.inc'"
             for SUBLEVEL in ${SUBLEVELS}
              do
               echo "    # >>> ${BASENAME}${SUBLEVEL}SIZEMIN=\"\$(( <size> + <extra> ))\""
             done
             echo "    for SRC in /root/bin/luks_*.inc ; do . \"\${SRC}\" ; done"
             for SUBLEVEL in ${SUBLEVELS}
              do
               echo "    lvm pvresize --setphysicalvolumesize \"\$(echo \"\${${BASENAME}${SUBLEVEL}SIZEMIN}m\")\" '$(eval echo "\${${BASENAME}${SUBLEVEL}}")'"
               echo "    shrink_fs_subdevs '${BASENAME}' '${SUBLEVEL}'"
             done
             echo '  (**)'
             lvm lvs --units m -o +devices "${VG}"
             echo "Shrinking volume ${DEV}..."
             lvm lvresize --size ${SIZE}m "${DEV}" || true
             printf -- '\a'
             return 1 2>/dev/null || exit 1
             ;;
       (luks) SUBSIZE="$(( ${SIZE} * 1024 * 2 ))"  # Convert MiB to 512-byte-sectors
              cryptsetup resize --size="${SUBSIZE}" "$(eval echo "\"\${${BASENAME}${LEVEL}DMNAME}\"")" || { return 1 2>/dev/null || exit 1 ; }
              ;;
       (mdadm) SUBTYPE="$(eval echo "\"\${${BASENAME}${LEVEL}METADATA}\"")"
               # "grow" raid array
               mdadm --grow "${DEV}" --bitmap=none
               if [ "${DEBVERSION}" -le 5 ]; then
                 SUBSIZE="$(( ${SIZE} * 1024 ))"
                 echo "Applying --grow ${DEV} --size=${SUBSIZE} in KiB"
                 mdadm --grow "${DEV}" --size=${SUBSIZE} || { return 1 2>/dev/null || exit 1 ; }
               else
                 echo "Applying --grow ${DEV} --size=${SIZE}M"
                 mdadm --grow "${DEV}" --size=${SIZE}M || { return 1 2>/dev/null || exit 1 ; }
               fi
               mdadm_wait "${DEV}" ; printf -- '\a' ; # TAKES TIME!!!
               if [ "${SUBTYPE}" = '0.90' -o "${SUBTYPE}" = '1.0' -o "${DEBVERSION}" -ge 7 ]; then
                 mdadm --grow "${DEV}" --bitmap=internal
               fi
               # update device sizes
               echo '> ATTENTION! mdadm subdevices have to be manually removed, resized and added'
               echo '  again, due to loosing consistency when shrinking subdevices below their'
               echo '  known device size.'
               mdadm_new_devicesize_via_remove_add "${BASENAME}" "${LEVEL}" "${DEV}" "${SUBTYPE}" 'shrink_fs_subdevs' 'X'
               echo "shrink_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
               printf -- '\a'
               return 1 2>/dev/null || exit 1
               ;;
       (mbr) SUBDEVS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
             PART="$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")"
             SUBSIZE="$(( ${SIZE} * 1024 ))"
             SUBTYPE="$([ "${PART}" -le 4 ] && echo 'p' || echo 'l')"
             [ -z "${FDISKCPARM}" ] || sfdisk -l -u S -q ${SUBDEVS}
             echo "> ${BASENAME} partition is ${PART}"
             if [ -z "${FDISKCPARM}" ]; then
               echo ">  [c]  # toggle DOS mode on/off, depends where the root partition starts, e.g. start sector 63 needs DOS compatibility on"
               echo ">  p"
             fi
             echo ">  d --> ${PART}"
             echo ">  n --> ${SUBTYPE} --> ${PART} --> Start Sector of partition ${PART} from previous p output (normally just <Enter>)"
             echo -n '   --> '
             if [ "${DEBVERSION}" -ge 6 ]; then
               SUBSIZE="$(( ${SIZE} * 1024 ))"
               echo -n "+${SUBSIZE}K or "
             fi
             echo -n '+(sectors-1)'
             for SECSIZE in 512 1024 2048 4096
              do
               SUBSIZE="$(( ( ${SIZE} * 1024 * 1024 ) / ${SECSIZE} ))"
               echo -n " ${SUBSIZE}s(${SECSIZE})"
             done
             printf -- '\n'
             echo "   Do *NOT* remove any signatures, otherwise you will destroy your data."
             echo ">  t --> ${PART} --> Type Code from previous p output"
             echo ">  p"
             echo ">  w (otherwise abort with q)"
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             if [ "${RESTART}" = 'D' ]; then
               echo ">  and continue with SUBDEVICE='${DEV}' ; LEVEL='${LEVEL}'"
             else
               echo ">  shrink_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
             fi
             fdisk -u ${FDISKCPARM} ${SUBDEVS}
             unset -v SUBSIZE
             unset -v SECSIZE
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             if [ "${RESTART}" = 'D' ]; then
               echo ">  and continue with SUBDEVICE='${DEV}' ; LEVEL='${LEVEL}'"
             else
               echo ">  shrink_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
             fi
             return 1 2>/dev/null || exit 1
             ;;
       (gpt) SUBDEVS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
             PART="$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")"
             sgdisk -p ${SUBDEVS}
             echo "> ${BASENAME} partition is ${PART}"
             echo ">  d --> ${PART}"
             echo ">  n --> ${PART} --> Start Sector of partition ${PART} from previous p output (normally just <Enter>)"
             echo "   --> +${SIZE}M --> Type Code of partition ${PART} from previous p output"
             echo "   Do *NOT* remove any signatures, otherwise you will destroy your data."
             echo ">  p"
             echo ">  w (otherwise abort with q)"
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             if [ "${RESTART}" = 'D' ]; then
               echo ">  and continue with SUBDEVICE='${DEV}' ; LEVEL='${LEVEL}'"
             else
               echo ">  shrink_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
             fi
             gdisk ${SUBDEVS}
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             if [ "${RESTART}" = 'D' ]; then
               echo ">  and continue with SUBDEVICE='${DEV}' ; LEVEL='${LEVEL}'"
             else
               echo ">  shrink_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
             fi
             return 1 2>/dev/null || exit 1
             ;;
       (*) echo "> ATTENTION! Type \"${TYPE}\" of ${DEV} is not supported or unknown, therefore"
           echo '  it has to be handled manually/individually if any action is required.'
           echo "shrink_fs_subdevs ${BASENAME} '$(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")'"
           printf -- '\a'
           return 1 2>/dev/null || exit 1
           ;;
      esac
    fi

    if [ "${SKIP}" -eq 0 -o "${RESTART}" != 'U' ]; then
      RC=0
      for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
       do
        shrink_fs_subdevs "${BASENAME}" "${STARTLEVELS}" "${RESTART}" "${SUBLEVEL}" "${LEVEL:-0}" || RC="$(( ${RC} + ${?} ))"
      done
      [ "${RC}" -ne 0 ] && { return 1 2>/dev/null || exit 1 ; }
    fi

    # Direction: bottom up
    if [ "${SKIP}" -eq 0 -o "${RESTART}" != 'D' ]; then
      case "${TYPE}" in
       (ext2|ext3|ext4) ;;
       (lvm) ;;
       (luks) ;;
       (mdadm) ;;
       (mbr|gpt) ;;
       (*) echo "> ATTENTION! Type \"${TYPE}\" of ${DEV} is not supported or unknown, therefore"
           echo '  it has to be handled manually/individually if any action is required.'
           echo "shrink_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
           printf -- '\a'
           return 1 2>/dev/null || exit 1
           ;;
      esac
    fi
  done

  return 0
}

maximize_fs_subdevs () {
  # Direction: bottom up; special case for mdadm partitions with metadata 0.90/1.0
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Start Level, e.g. _1
  # 3 = Restart Direction, e.g. [U]p or D[own], default [ ] for both
  # 4 = Level, e.g. _1
  # 5 = Upper Level, e.g. _1

  local BASENAME
  local STARTLEVELS
  local RESTART
  local LEVELS
  local UPPERLEVEL

  local LEVEL
  local SKIP
  local SKIPVALUE
  local SUBLEVEL
  local DEV
  local DEVALT
  local PART
  local SUBDEVS
  local TYPE
  local SUBTYPE
  local RC

  if [ -z "${1:-}" ]; then
    echo "Usage: maximize_fs_subdevs <var name> [<start level prefix> [<restart direction>]]" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  BASENAME="${1}"
  STARTLEVELS="${2:-}"
  RESTART="${3:-}"
  LEVELS="${4:-0}"
  UPPERLEVEL="${5:-}"

  for LEVEL in ${LEVELS}
   do
    [ "${LEVEL}" != '0' ] || LEVEL=''

    SKIP=0
    if [ -n "${STARTLEVELS}" ]; then
      if [ "${RESTART}" = 'D' ]; then
        SKIP=1
        SKIPVALUE=0
      else
        SKIPVALUE=1
      fi
      #
      for SUBLEVEL in ${STARTLEVELS}
       do
# eval hack, due to shells not being able to handle ORs (=pipe symbol) in variables
        eval "case \"${LEVEL:-0}\" in
               (${SUBLEVEL}*) SKIP="${SKIPVALUE}"
                              break
                              ;;
              esac"
      done
    fi

    DEV="$(eval echo "\"\${${BASENAME}${LEVEL}}\"")"
    TYPE="$(eval echo "\"\${${BASENAME}${LEVEL}TYPE}\"")"

    # Direction: top down
    if [ "${SKIP}" -eq 0 -a "${RESTART}" != 'U' -a "${RESTART}" != 'U2' ]; then
      case "${TYPE}" in
       (ext2|ext3|ext4) ;;
       (lvm) ;;
       (luks) ;;
       (mdadm) SUBTYPE="$(eval echo "\"\${${BASENAME}${LEVEL}METADATA}\"")"
               # special case: update device sizes differently
               if [ "${SUBTYPE}" = '0.90' -o "${SUBTYPE}" = '1.0' ]; then
                 echo ">> MAXIMIZE level ${LEVEL:-top}: ${DEV} via ${TYPE}"
                 echo "> ATTENTION! mdadm super block of version ${SUBTYPE} found which is located at the"
                 echo '  end of its subdevices, therefore devices have to be manually removed,'
                 echo '  resized and added again.'
                 echo '  (see https://lists.fedoraproject.org/pipermail/users/2010-May/373580.html #15)'
                 mdadm_new_devicesize_via_remove_add "${BASENAME}" "${LEVEL}" "${DEV}" "${SUBTYPE}" 'maximize_fs_subdevs'
                 echo "maximize_fs_subdevs '${BASENAME}' '${LEVEL}' 'U'"
                 printf -- '\a'
                 return 1 2>/dev/null || exit 1
               fi
               ;;
       (mbr|gpt) ;;
       (*) echo "> ATTENTION! Type \"${TYPE}\" of ${DEV} is not supported or unknown, therefore"
           echo '  it has to be handled manually/individually if any action is required.'
           echo "maximize_fs_subdevs '${BASENAME}' '$(eval echo "\${${BASENAME}${LEVEL}SUBLEVELS}")'"
           printf -- '\a'
           return 1 2>/dev/null || exit 1
           ;;
      esac
    fi

    if [ "${SKIP}" -eq 0 -o "${RESTART}" = 'D' ]; then
      RC=0
      for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
       do
        maximize_fs_subdevs "${BASENAME}" "${STARTLEVELS}" "${RESTART}" "${SUBLEVEL}" "${LEVEL:-0}" || RC="$(( ${RC} + ${?} ))"
      done
      [ "${RC}" -eq 0 ] || { return 1 2>/dev/null || exit 1 ; }
    fi

    # Direction: bottom up
    if [ "${SKIP}" -eq 0 -o "${RESTART}" != 'D' ]; then
      echo ">> MAXIMIZE level ${LEVEL:-top}: ${DEV} via ${TYPE}"
      case "${TYPE}" in
       (ext2|ext3|ext4) resize2fs "${DEV}"
                        fsck -f "${DEV}"
                        ;;
       (lvm) echo '> ATTENTION! It is not recommended to maximize volumes without need,'
             echo '  therefore lvm volumes, groups and devs have to be handled manually/individually.'
             echo "  (*) lvm vgs --units m '$(eval echo "\${${BASENAME}${LEVEL}VG}")'"
             if [ "${DEBVERSION}" -ge 5 ]; then
               echo "      lvm lvresize -l 50%VG '${DEV}'"
             fi
             echo "      lvm lvresize -L 10g '${DEV}'"
             echo "      maximize_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
             SUBDEVS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
             echo "Resizing devices ${SUBDEVS}..."
             lvm pvresize ${SUBDEVS}
             printf -- '\a'
             return 1 2>/dev/null || exit 1
             ;;
       (luks) cryptsetup resize "$(eval echo "\${${BASENAME}${LEVEL}DMNAME}")" || { return 1 2>/dev/null || exit 1 ; }
              ;;
       (mdadm) SUBTYPE="$(eval echo "\"\${${BASENAME}${LEVEL}METADATA}\"")"
               # update device sizes
               if [ "${SUBTYPE}" = '0.90' -o "${SUBTYPE}" = '1.0' ]; then
                 :
               elif [ "${DEBVERSION}" -le 6 -a "${RESTART}" != 'D' -a "${RESTART}" != 'U2' ]; then
                 RC=0
                 if [ "${DEBVERSION}" -le 4 ]; then
                   echo "> ATTENTION! mdadm of Debian ${DEBVERSION} doesn't provide --update=devicesize,"
                   echo '  therefore devices have to be manually removed, resized and added again.'
                   echo '  (see https://lists.fedoraproject.org/pipermail/users/2010-May/373580.html #15)'
                   RC=99
                 else
                   stop_fs_subdevs "${BASENAME}" "${LEVEL}"
                   mdadm --stop "${DEV}" || RC="${?}"
                 fi
                 if [ "${RC}" -ne 0 ]; then
                   mdadm_new_devicesize_via_remove_add "${BASENAME}" "${LEVEL}" "${DEV}" "${SUBTYPE}" 'maximize_fs_subdevs'
                   echo "maximize_fs_subdevs '${BASENAME}' '${LEVEL}' 'U2'"
                   printf -- '\a'
                   return 1 2>/dev/null || exit 1
                 fi
                 echo "  Using --assemble ${DEV} --update=devicesize"
                 DEVALT="$(eval echo "\"\${${BASENAME}${LEVEL}ALT}\"")"
                 mdadm --assemble "${DEV}" --update=devicesize || mdadm --assemble "${DEVALT}" --update=devicesize || { return 1 2>/dev/null || exit 1 ; }
               fi
               # grow raid array
               if [ "${DEBVERSION}" -le 4 -a "${SUBTYPE}" != '0.90' -a "${SUBTYPE}" != '1.0' ]; then
                 echo "> ATTENTION! mdadm of Debian ${DEBVERSION} will destroy raids with super block of version ${SUBTYPE}"
                 echo '  when growing with --size=max, therefore calculate the new size manually.'
                 echo '  Only use full MiBs and leave several MiBs unused so that growing will not fail.'
                 echo '  Note that size has to be specified in KiB.'
                 echo "mdadm --grow '${DEV}' --size=..."
                 echo "mdadm_wait '${DEV}' ; mdadm --detail '${DEV}' ; printf -- '\\a' ; # TAKES TIME!!!"
                 echo "start_fs_subdevs '${BASENAME}'"
                 echo "maximize_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
                 printf -- '\a'
                 return 1 2>/dev/null || exit 1
               else
                 mdadm --grow "${DEV}" --bitmap=none
                 echo "  Using --grow ${DEV} --size=max"
                 mdadm --grow "${DEV}" --size=max || { return 1 2>/dev/null || exit 1 ; }
                 mdadm_wait "${DEV}" ; printf -- '\a' ; # TAKES TIME!!!
                 if [ "${SUBTYPE}" = '0.90' -o "${SUBTYPE}" = '1.0' -o "${DEBVERSION}" -ge 7 ]; then
                   mdadm --grow "${DEV}" --bitmap=internal
                 fi
                 start_fs_subdevs "${BASENAME}"
               fi
               :  # NOP to kill exit code
               ;;
       (mbr) SUBDEVS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
             PART="$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")"
             SUBTYPE="$([ "${PART}" -le 4 ] && echo 'p' || echo 'l')"
             [ -z "${FDISKCPARM}" ] || sfdisk -l -u S -q ${SUBDEVS}
             echo "> ${BASENAME} partition is ${PART}"
             if [ -z "${FDISKCPARM}" ]; then
               echo ">  [c]  # toggle DOS mode on/off, depends where the root partition starts, e.g. start sector 63 needs DOS compatibility on"
               echo ">  p"
             fi
             echo ">  d --> ${PART}"
             echo ">  n --> ${SUBTYPE} --> ${PART} --> Start Sector of partition ${PART} from previous p output (normally just <Enter>)"
             echo "   --> Last available sector (normally just <Enter>)"
             echo "   Do *NOT* remove any signatures, otherwise you will destroy your data."
             echo ">  t --> ${PART} --> Type Code from previous p output"
             echo ">  p"
             echo ">  w (otherwise abort with q)"
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             if [ "${RESTART}" = 'D' ]; then
               echo ">  and continue with SUBDEVICE='${DEV}' ; LEVEL='${LEVEL}'"
             else
               echo ">  maximize_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
             fi
             fdisk -u ${FDISKCPARM} ${SUBDEVS}
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             if [ "${RESTART}" = 'D' ]; then
               echo ">  and continue with SUBDEVICE='${DEV}' ; LEVEL='${LEVEL}'"
             else
               echo ">  maximize_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
             fi
             return 1 2>/dev/null || exit 1
             ;;
       (gpt) SUBDEVS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
             PART="$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")"
             sgdisk -p ${SUBDEVS}
             echo "> ${BASENAME} partition is ${PART}"
             echo ">  d --> ${PART}"
             echo ">  n --> ${PART} --> Start Sector of partition ${PART} from previous p output (normally just <Enter>)"
             echo "   --> Last available sector (normally just <Enter>)"
             echo "   --> Type Code of partition ${PART} from previous p output"
             echo "   Do *NOT* remove any signatures, otherwise you will destroy your data."
             echo ">  p"
             echo ">  w (otherwise abort with q)"
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             if [ "${RESTART}" = 'D' ]; then
               echo ">  and continue with SUBDEVICE='${DEV}' ; LEVEL='${LEVEL}'"
             else
               echo ">  maximize_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
             fi
             gdisk ${SUBDEVS}
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             if [ "${RESTART}" = 'D' ]; then
               echo ">  and continue with SUBDEVICE='${DEV}' ; LEVEL='${LEVEL}'"
             else
               echo ">  maximize_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
             fi
             return 1 2>/dev/null || exit 1
             ;;
       (*) echo "> ATTENTION! Type \"${TYPE}\" of ${DEV} is not supported or unknown, therefore"
           echo '  it has to be handled manually/individually if any action is required.'
           echo "maximize_fs_subdevs '${BASENAME}' '${UPPERLEVEL}' 'U'"
           printf -- '\a'
           return 1 2>/dev/null || exit 1
           ;;
      esac
    fi
  done

  return 0
}

mdadm_new_devicesize_via_remove_add () {
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Level, e.g. _1
  # 3 = Device, e.g. /dev/md1
  # 4 = Meta Data Version, e.g. 0.90
  # 5 = Function Name
  # 6 = Flag Do Subdevices

  local BASENAME
  local LEVEL
  local DEV
  local SUBTYPE
  local FNAME
  local DOSUBDEVS

  local SUBCOUNT
  local SUBLEVEL
  local SUBDOWNCOUNT
  local SUBDEV

  BASENAME="${1}"
  LEVEL="${2}"
  DEV="${3}"
  SUBTYPE="${4}"
  FNAME="${5}"
  DOSUBDEVS="${6}"

  if [ "${DEBVERSION}" -ge 7 -a "${SUBTYPE}" != '0.90' -a "${SUBTYPE}" != '1.0' ]; then
    echo ''
    echo "mdadm --grow '${DEV}' --bitmap=internal"
    echo ''
  fi

  echo '  Do the following for each subdevice, but only one subdevice at a time.'
  SUBCOUNT=0
  for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
   do
    SUBCOUNT="$(( ${SUBCOUNT} + 1 ))"
    SUBDOWNCOUNT="${SUBCOUNT}"
    for SUBDEV in $(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")
     do
      [ "${SUBDOWNCOUNT}" -eq 1 ] && break
      SUBDOWNCOUNT="$(( ${SUBDOWNCOUNT} - 1 ))"
    done
    echo "SUBDEVICE='${SUBDEV}' ; LEVEL='${SUBLEVEL}'"
  done
  echo '# do for each subdevice'
  echo "mdadm '${DEV}' --fail \"\${SUBDEVICE}\""
  echo "mdadm '${DEV}' --remove \"\${SUBDEVICE}\""
  if [ "${SUBTYPE}" = '0.90' -o "${SUBTYPE}" = '1.0' -o "${DEBVERSION}" -le 6 ]; then
    echo 'mdadm --zero-superblock "${SUBDEVICE}"'
  fi
  if [ "${SUBTYPE}" = '0.90' -o "${SUBTYPE}" = '1.0' -o -n "${DOSUBDEVS}" ]; then
    echo "${FNAME} '${BASENAME}' \"\${LEVEL}\" 'D'"
    echo 'partprobe || blockdev --rereadpt "${SUBDEVICE}" ; # otherwise reboot'
    echo 'for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
    echo "SUBDEVICE='...'"
  fi
  echo '# Monitoring is possible in another session via "watch -- cat /proc/mdstat".'
  if [ "${SUBTYPE}" = '0.90' -o "${SUBTYPE}" = '1.0' -o "${DEBVERSION}" -le 6 ]; then
    echo "mdadm '${DEV}' --add \"\${SUBDEVICE}\""
  else
    echo "mdadm '${DEV}' --re-add --update=devicesize \"\${SUBDEVICE}\""
  fi
  echo "mdadm_wait '${DEV}' ; mdadm --detail '${DEV}' ; printf -- '\\a' ; # TAKES TIME!!!"
  echo "# repeat with next subdevice"
  echo ''
  echo "start_fs_subdevs '${BASENAME}'"

  return 0
}

remove_fs_subdevs () {
  # Direction: top down
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Stop Level, e.g. _1
  # 3 = Level, e.g. _1
  # 4 = Restart Direction, e.g. [U]p or D[own], default [ ] for both

  local BASENAME
  local STOPLEVELS
  local LEVELS
  local RESTART

  local LEVEL
  local SKIP
  local DEV
  local TYPE
  local DMNAME
  local PART

  local LV
  local VG
  local SUBDEVS
  local SUBLEVELS

  local SUBLEVEL

  if [ -z "${1:-}" ]; then
    echo "Usage: remove_fs_subdevs <var name> [<stop level prefix> [level prefix> [<restart direction>]]]" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  BASENAME="${1}"
  STOPLEVELS="${2:-}"
  LEVELS="${3:-0}"
  RESTART="${4:-}"

  for LEVEL in ${LEVELS}
   do
    [ "${LEVEL}" != '0' ] || LEVEL=''

    SKIP=0
    if [ -n "${STOPLEVELS}" ]; then
      for SUBLEVEL in ${STOPLEVELS}
       do
# eval hack, due to shells not being able to handle ORs (=pipe symbol) in variables
        eval "case \"${LEVEL:-0}\" in
               (${SUBLEVEL}*) SKIP=1
                              break
                              ;;
              esac"
      done
    fi

    DEV="$(eval echo "\"\${${BASENAME}${LEVEL}}\"")"
    TYPE="$(eval echo "\"\${${BASENAME}${LEVEL}TYPE}\"")"

    echo ">> REMOVE level ${LEVEL:-top}: ${DEV} via ${TYPE}"
    if [ "${SKIP}" -ne 0 ]; then
      echo '...skipping'
    else
      case "${TYPE}" in
       (ext2|ext3|ext4|swap) ;;
       (lvm) LV="$(eval echo "\"\${${BASENAME}${LEVEL}LV}\"")"
             VG="$(eval echo "\"\${${BASENAME}${LEVEL}VG}\"")"
             SUBDEVS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
             SUBLEVELS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")"
             echo "> ATTENTION! ${BASENAME} is installed on block devices that use lvm."
             echo "  LVM setups can be very complex(*) therefore volume groups and"
             echo "  physical volumes have to be handled manually/individually."
             echo "  (*) multiple volumes in one group and/or multiple devs"
             echo "      with different sizes in one group."
             echo "  Logical volume '${LV}' in volume group '${VG}' via '${SUBDEVS}'."
             if [ "${RESTART}" != 'D' ]; then
               echo "  The logical volume will be removed as requested."
             fi
             echo "  Check if the list output at the bottom(**) shows other logical volumes"
             echo "  in the volume group. If not, then - if wanted - remove the volume group,"
             echo "  its phyiscal volumes and continue with removing the sub devices."
             echo "    lvm vgreduce '${VG}' ${SUBDEVS} || lvm vgremove '${VG}'"
             echo "    lvm pvremove ${SUBDEVS}"
             echo "    remove_fs_subdevs '${BASENAME}' '${STOPLEVELS}' '${SUBLEVELS}'"
             echo "  Otherwise use the freed space to defrag the physical volume(s)"
             echo "  via $(if [ "${DEBVERSION}" -le 4 ]; then echo 'lvdisplay'; else echo 'pvdisplay'; fi) -m and pvmove, which can also be done in pieces/chunks."
             echo "  If a physical volume is totally freed it can also be removed via vgreduce."
             echo "  After that derive the new necessary sizes of the physical volume(s)"
             echo "  and maintain these sizes in /root/bin/luks_${BASENAME}.inc"
             echo "  Reload definitions, resize physical volume(s) via pvresize and continue"
             echo "  with shrinking the sub devices."
             if [ "${DEBVERSION}" -le 4 ]; then
               echo "    lvm lvdisplay -v -m"
             fi
             echo "    lvm pvdisplay -v -m ${SUBDEVS} ; #or lvm pvs -v --units m --segments ${SUBDEVS}"
             echo "    lvm pvmove --alloc anywhere -n <lv> <dev>[:<oldstart>+<size>] <dev>:<newstart>+<size>"
             echo "    # optionally remove cleared pv from vg and pv itself"
             echo "    #lvm vgreduce '${VG}' <pv>"
             echo "    #lvm pvremove <pv>"
             echo "    #remove_fs_subdevs '${BASENAME}' '${STOPLEVELS}' <level of pv, see below pvresize>"
             echo "    vgs --units m -o vg_name,vg_size,vg_free '${VG}'"
             echo "    pvs --units m -o pv_name,pv_size,pv_free,pv_used ${SUBDEVS}"
             echo "    nano '/root/bin/luks_${BASENAME}.inc'"
             for SUBLEVEL in ${SUBLEVELS}
              do
               echo "    # >>> ${BASENAME}${SUBLEVEL}SIZEMIN=\"\$(( <size> + <extra> ))\""
             done
             echo "    for SRC in /root/bin/luks_*.inc ; do . \"\${SRC}\" ; done"
             for SUBLEVEL in ${SUBLEVELS}
              do
               echo "    lvm pvresize --setphysicalvolumesize \"\$(echo \"\${${BASENAME}${SUBLEVEL}SIZEMIN}m\")\" '$(eval echo "\${${BASENAME}${SUBLEVEL}}")'"
               echo "    shrink_fs_subdevs '${BASENAME}' '${SUBLEVEL}'"
             done
             echo '  (**)'
             lvm lvs --units m -o +devices "${VG}"
             if [ "${RESTART}" != 'D' ]; then
               echo "Removing volume ${DEV}..."
               lvm lvremove "${DEV}" || {
                 echo "  Failed to remove ${DEV}, therefore clean first 8 MiB of it and disable RESUME."
                 echo "    dd if=/dev/zero of='${DEV}' bs=1M count=8"
                 echo "    sed -i -e 's#^[[:space:]]*\(RESUME=\)#\#\1#' /etc/initramfs-tools/conf.d/resume"
                 echo '    update-initramfs -u -k all'
                 echo '    # if any initrd update fails, just create a new one from scratch'
                 echo '     IVERSION="$(uname -r)" ; # FIXME: set to problematic initramfs version'
                 echo '     rm -f /boot/initrd.img-${IVERSION} ; update-initramfs -c -k ${IVERSION}'
                 echo '    #'
                 echo '    reboot'
                 echo '  and try again'
                 echo '    for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
                 echo "    remove_fs_subdevs '${BASENAME}' '' '${LEVEL}'"
               }
             fi
             printf -- '\a'
             return 1 2>/dev/null || exit 1
             ;;
       (luks) DMNAME="$(eval echo "\"\${${BASENAME}${LEVEL}DMNAME}\"")"
              if [ "${DEBVERSION}" -le 4 ]; then
                cryptsetup luksClose "${DMNAME}"
              else
                cryptdisks_stop "${DMNAME}"
              fi
              sed -i -e "/^[[:space:]]*${DMNAME}[[:space]]\+/d" /etc/crypttab
              ;;
       (mdadm) PART="$(eval echo "\"\${${BASENAME}${LEVEL}ALT}\"")"
               mdadm --stop "${DEV}"
               mdadm --zero-superblock "$(eval echo "\${${BASENAME}${LEVEL}SUBDEVS}")"
               sed -i -e "\%^ARRAY[[:space:]]\+${DEV}[[:space:]]\+%d" /etc/mdadm/mdadm.conf
               [ ! -e "${DEV}" ] || rm "${DEV}"
               if [ -n "${PART}" ]; then
                 sed -i -e "\%^ARRAY[[:space:]]\+${PART}[[:space:]]\+%d" /etc/mdadm/mdadm.conf
                 [ ! -e "${PART}" ] || rm "${PART}"
               fi
               ;;
       (mbr) SUBDEVS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
             PART="$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")"
             [ -z "${FDISKCPARM}" ] || sfdisk -l -u S -q ${SUBDEVS}
             echo "> ${BASENAME} partition is ${PART}"
             [ -n "${FDISKCPARM}" ] || echo ">  p"
             echo ">  d --> ${PART}"
             echo ">  p"
             echo ">  w (otherwise abort with q)"
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             fdisk -u ${FDISKCPARM} ${SUBDEVS}
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             ;;
       (gpt) SUBDEVS="$(eval echo "\"\${${BASENAME}${LEVEL}SUBDEVS}\"")"
             PART="$(eval echo "\"\${${BASENAME}${LEVEL}SUBPART}\"")"
             sgdisk -p ${SUBDEVS}
             echo "> ${BASENAME} partition is ${PART}"
             echo ">  d --> ${PART}"
             echo ">  p"
             echo ">  w (otherwise abort with q)"
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             gdisk ${SUBDEVS}
             echo ">  ATTENTION! partprobe || blockdev --rereadpt ${SUBDEVS} ; # otherwise reboot"
             echo '>  for SRC in /root/bin/luks_*.inc ; do . "${SRC}" ; done'
             ;;
       (*) echo "> ATTENTION! Type \"${TYPE}\" of ${DEV} is not supported or unknown, therefore"
           echo '  it has to be handled manually/individually if any action is required.'
           printf -- '\a'
           return 1 2>/dev/null || exit 1
           ;;
      esac
    fi

    for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
     do
      remove_fs_subdevs "${BASENAME}" "${STOPLEVELS}" "${SUBLEVEL}" "${RESTART}" || { return 1 2>/dev/null || exit 1 ; }
    done
  done

  return 0
}

wait_fs_subdevs () {
  # Direction: top down
  # 1 = Base Name of Variable, e.g. OLDROOT
  # 2 = Level, e.g. _1

  local BASENAME
  local LEVELS

  local LEVEL
  local DEV
  local TYPE
  local MDADM

  local SUBLEVEL

  if [ -z "${1:-}" ]; then
    echo "Usage: wait_fs_subdevs <var name>" 1>&2
    return 1 2>/dev/null || exit 1
  fi

  BASENAME="${1}"
  LEVELS="${2:-0}"

  unset -v MDADM

  for LEVEL in ${LEVELS}
   do
    [ "${LEVEL}" != '0' ] || LEVEL=''

    DEV="$(eval echo "\"\${${BASENAME}${LEVEL}}\"")"
    TYPE="$(eval echo "\"\${${BASENAME}${LEVEL}TYPE}\"")"

    echo ">> WAIT level ${LEVEL:-top}: ${DEV} via ${TYPE}"
    case "${TYPE}" in
     (ext2|ext3|ext4) ;;
     (lvm) ;;
     (luks) ;;
     (mdadm) MDADM="${MDADM:+${MDADM} }${DEV}"
             ;;
     (mbr|gpt) ;;
     (*) echo "> ATTENTION! Type \"${TYPE}\" of ${DEV} is not supported or unknown, therefore"
         echo '  it has to be handled manually/individually if any action is required.'
         printf -- '\a'
         ;;
    esac

    for SUBLEVEL in $(eval echo "\"\${${BASENAME}${LEVEL}SUBLEVELS}\"")
     do
      wait_fs_subdevs "${BASENAME}" "${SUBLEVEL}"
    done
  done

  if [ -n "${MDADM:-}" ]; then
    mdadm_wait ${MDADM} ; # TAKES TIME!!!
    printf -- '\a'
  fi

  return 0
}
