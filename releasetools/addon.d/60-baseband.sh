#!/sbin/sh
# 
# /system/addon.d/60-baseband.sh
# During a CM9 upgrade, this script backs up baseband config,
# /system is erased and reinstalled, then this script restore a backup list.
#

. /tmp/backuptool.functions

list_files() {
cat <<EOF
etc/motorola/bp_nvm_default/File_GSM
etc/motorola/bp_nvm_default/File_GPRS
etc/motorola/bp_nvm_default/File_UMA
etc/motorola/bp_nvm_default/File_Logger
etc/motorola/bp_nvm_default/File_Seem_Flex_Tables
etc/motorola/bp_nvm_default/File_Audio
etc/motorola/bp_nvm_default/File_Audio1
etc/motorola/bp_nvm_default/File_Audio2
etc/motorola/bp_nvm_default/File_Audio3
etc/motorola/bp_nvm_default/File_Audio4
etc/motorola/bp_nvm_default/File_Audio5
etc/motorola/bp_nvm_default/File_Audio6
etc/motorola/bp_nvm_default/File_Audio7
etc/motorola/bp_nvm_default/File_Audio8
etc/motorola/bp_nvm_default/File_Audio1_AMR_WB
etc/motorola/bp_nvm_default/File_Audio2_AMR_WB
etc/motorola/bp_nvm_default/File_Audio3_AMR_WB
etc/motorola/bp_nvm_default/File_Audio4_AMR_WB
etc/motorola/bp_nvm_default/File_Audio5_AMR_WB
etc/motorola/bp_nvm_default/generic_pds_init
EOF
}

case "$1" in
  backup)
    list_files | while read FILE DUMMY; do
      backup_file $S/"$FILE"
    done
  ;;
  restore)
    list_files | while read FILE REPLACEMENT; do
      R=""
      [ -n "$REPLACEMENT" ] && R="$S/$REPLACEMENT"
      [ -f "$C/$S/$FILE" ] && restore_file $S/"$FILE" "$R"
    done
  ;;
  pre-backup)
    # Stub
  ;;
  post-backup)
    # Stub
  ;;
  pre-restore)
    # Stub
  ;;
  post-restore)
    # Stub
  ;;
esac
