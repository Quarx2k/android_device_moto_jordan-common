#!/system/bin/sh

# stop panic_daemon restarting
setprop ctl.stop panic_daemon

# let UI handle the failure
am startservice -a com.cyanogenmod.settings.device.action.HANDLE_BP_PANIC -n com.cyanogenmod.settings.device/.BpPanicHandlerService 2>&1 | grep Error
touch /cache/bp_panic

#if [ $? -eq 0 ]; then
    # some error occured, use fallback and reboot directly
#    reboot
#fi
