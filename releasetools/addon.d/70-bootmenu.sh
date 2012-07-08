#!/sbin/sh
# 
# /system/addon.d/70-bootmenu.sh
# During a CM9 nightly upgrades, this script backs up bootmenu config,
# /system is erased and reinstalled, then this script restore a backup list.
#

. /tmp/backuptool.functions

list_files() {
cat <<EOF
bootmenu/config/overclock.conf
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
