package com.cyanogenmod.defyparts;

import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

public class BpPanicReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        final String action = intent.getAction();

        if (Intent.ACTION_BOOT_COMPLETED.equals(action)) {
            SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
            if (prefs.getBoolean(BpPanicHandlerService.KEY_NEED_REBOOT_NOTICE, false)) {
                Intent noticeIntent = new Intent(context, BpPanicRebootNoticeActivity.class);
                noticeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

                context.startActivity(noticeIntent);
                prefs.edit().putBoolean(BpPanicHandlerService.KEY_NEED_REBOOT_NOTICE, false).commit();
            }
        }
    }
}
