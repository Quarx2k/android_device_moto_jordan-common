#!/sbin/sh
# 
# /system/addon.d/70-modules.sh
# During a CM9 nightly upgrades, this script backs up kernel modules,
# /system is erased and reinstalled, then this script restore a backup list.
#

. /tmp/backuptool.functions

list_files() {
cat <<EOF
lib/modules/act_gact.ko
lib/modules/act_mirred.ko
lib/modules/act_police.ko
lib/modules/cls_u32.ko
lib/modules/em_u32.ko
lib/modules/ifb.ko
lib/modules/kineto_gan.ko
lib/modules/modem_pm_driver.ko
lib/modules/netmux.ko
lib/modules/netmux_linkdriver.ko
lib/modules/output.ko
lib/modules/pcbc.ko
lib/modules/sch_htb.ko
lib/modules/sch_ingress.ko
lib/modules/sec.ko
lib/modules/vpnclient.ko
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
