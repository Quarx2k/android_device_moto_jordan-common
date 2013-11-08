#!/system/bootmenu/binary/busybox ash

######## BootMenu Script
######## Overclock.sh

export PATH=/sbin:/system/xbin:/system/bin
CONFIG_FILE="/system/bootmenu/config/overclock.conf"
MODULE_DIR="/system/lib/modules"
SCALING_GOVERNOR="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
ASKED_MODE=$1

#############################################################
# Parameters Load
#############################################################

##### Default Param
#enable 1
#scaling 0
#sched 1
#clk1 300
#clk2 600
#clk3 800
#clk4 1000
#vsel1 33
#vsel2 48
#vsel3 58
#vsel4 62

param_load()
{
  for CONF in $(sed -e 's/^\([^ ]*\) \(.*\)/\1=\2/g' $CONFIG_FILE); do
    export $CONF
  done
}

param_safe()
{
  echo "cpufreq: ondemand safe"
  # for bootmenu operations
  # enable ondemand profile
  # which is in kernel
  export enable=1
  export scaling=0
  export sched=1
  export clk1=300
  export clk2=600
  export clk3=800
  export clk4=1000
  export vsel1=33
  export vsel2=48
  export vsel3=58
  export vsel4=62
  echo 1 > /sys/power/sr_vdd2_autocomp
}

#############################################################
# Get Address
#############################################################

get_address()
{
  insmod $MODULE_DIR/cpufreq_stats.ko
  cpufreq_table=`grep -e omap2_clk_init_cpufreq_table /proc/kallsyms | sed -e "s/\([0-9A-Fa-f]\{8\}\).*/\1/"`
  stats_update=`grep -e cpufreq_stats_freq_update /proc/kallsyms | sed -e "s/\([0-9A-Fa-f]\{8\}\).*/\1/"`
}

#############################################################
# Install Module
#############################################################

install_module()
{
  # load module
  insmod $MODULE_DIR/overclock_defy.ko omap2_clk_init_cpufreq_table_addr=0x$cpufreq_table
  #set cpufreq_stats_update_addr
  echo 0x$stats_update > /proc/overclock/cpufreq_stats_update_addr
  busybox chown -R system /sys/devices/system/cpu
  busybox chown -R system /sys/class/block/mmc*/queue
}

#############################################################
# Set Scaling
#############################################################

set_scaling()
{
  case "$scaling" in
    "0" )
      echo "interactive" > $SCALING_GOVERNOR;;
    "1" )
      echo "ondemand" > $SCALING_GOVERNOR;;
    "2" )
      echo "performance" > $SCALING_GOVERNOR;;
    "3" )
      echo "powersave" > $SCALING_GOVERNOR;;
    "4" )
      echo "boosted" > $SCALING_GOVERNOR;;
    "5" )
      echo "smartass" > $SCALING_GOVERNOR;;
     * )
    ;;
  esac
}


#############################################################
# Alternative I/O Schedulers
#
# Default linux I/O Schedulers are optimized for Hard disks
# The alternative ones are optimized for flash disks
#############################################################

set_ioscheduler()
{
  case "$sched" in
    "0" )
      export iosched="cfq";;
    "1" )
      export iosched="deadline";;
    "2" )
      export iosched="sio";;
     * )
    ;;
  esac

  for i in /sys/block/mmc*/queue; do
      [ -f "$i/scheduler" ]                 && echo $iosched > $i/scheduler
  done

}

#############################################################
# Set Clock Table
#############################################################

set_overclock_table()
{
    echo "$vsel4" > /proc/overclock/max_vsel
    echo "${clk4}000" > /proc/overclock/max_rate
    echo "4 ${clk4}000000 $vsel4" > /proc/overclock/mpu_opps
    echo "3 ${clk3}000000 $vsel3" > /proc/overclock/mpu_opps
    echo "2 ${clk2}000000 $vsel2" > /proc/overclock/mpu_opps
    echo "1 ${clk1}000000 $vsel1" > /proc/overclock/mpu_opps
    echo "0 ${clk4}000" > /proc/overclock/freq_table
    echo "1 ${clk3}000" > /proc/overclock/freq_table
    echo "2 ${clk2}000" > /proc/overclock/freq_table
    echo "3 ${clk1}000" > /proc/overclock/freq_table
}

#############################################################
# Set GPU Freq
#############################################################

set_gpu_freq()
{
    echo 266666666 > /proc/gpu/max_rate  #${gpu_clk1} 266mhz
}

#############################################################
# Main Scrpit
#############################################################

if [ "$ASKED_MODE" = "safe" ]; then
  param_safe
else
  if [ -e $CONFIG_FILE ]; then
    param_load
  else
    param_safe
  fi
fi

if [ $enable -eq 1 ]; then

  get_address
  install_module

  echo "set scaling..."
  set_scaling
  echo "set overclock table..."
  set_overclock_table
  echo "set gpu freq..."
  set_gpu_freq
  echo "set ioscheduler..."
  set_ioscheduler

  busybox chown -R system /sys/devices/system/cpu

fi
