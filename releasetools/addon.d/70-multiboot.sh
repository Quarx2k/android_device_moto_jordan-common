#!/sbin/sh
# 
# /system/addon.d/70-multiboot.sh
# During a CM9 upgrade, this script backs up multiboot addon,
# /system is erased and reinstalled, then this script restore a backup list.
#

. /tmp/backuptool.functions

list_files() {
cat <<EOF
bootmenu/script/2nd-system.sh
bootmenu/config/multiboot.conf
bootmenu/binary/multiboot
bootmenu/2nd-system/2nd-init
bootmenu/2nd-system/errormessage
bootmenu/2nd-system/fshook.bootcyanogenrom.sh
bootmenu/2nd-system/fshook.bootrecovery.sh
bootmenu/2nd-system/fshook.bootstockrom.sh
bootmenu/2nd-system/fshook.config.sh
bootmenu/2nd-system/fshook.edit_devtree.sh
bootmenu/2nd-system/fshook.functions.sh
bootmenu/2nd-system/fshook.postfshook.sh
bootmenu/2nd-system/fshook.prevent_unmount.sh
bootmenu/2nd-system/init.hook.rc
bootmenu/2nd-system/recovery/sbin/recoveryexit.sh
bootmenu/2nd-system/recovery/res/images/android.png
bootmenu/2nd-system/recovery/res/images/icon_bootmenu.png
bootmenu/2nd-system/recovery/res/images/icon_clockwork.png
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
