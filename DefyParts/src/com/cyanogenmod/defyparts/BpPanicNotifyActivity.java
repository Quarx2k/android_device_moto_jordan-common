package com.cyanogenmod.defyparts;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;

public class BpPanicNotifyActivity extends Activity implements DialogInterface.OnClickListener {
    private static final int DIALOG_ACTION_SELECT = 1;

    private AlertDialog mDialog;

    private BroadcastReceiver mTimerUpdateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            int timeout = intent.getIntExtra(BpPanicHandlerService.EXTRA_TIMEOUT, -1);
            updateDialogMessage(timeout);
        }
    };

    @Override
    protected Dialog onCreateDialog(int id, Bundle args) {
        if (id == DIALOG_ACTION_SELECT) {
            mDialog = new AlertDialog.Builder(this)
                    .setTitle(R.string.modem_error)
                    .setMessage(R.string.modem_error_msg)
                    .setCancelable(false)
                    .setPositiveButton(R.string.reboot_btn, this)
                    .setNeutralButton(R.string.snooze_btn, this)
                    .setNegativeButton(R.string.dismiss_btn, this)
                    .create();
            return mDialog;
        }

        return super.onCreateDialog(id, args);
    }

    @Override
    protected void onPrepareDialog(int id, Dialog dialog, Bundle args) {
        if (id == DIALOG_ACTION_SELECT) {
            updateDialogMessage(-1);
        }
        super.onPrepareDialog(id, dialog, args);
    }

    @Override
    protected void onResume() {
        super.onResume();
        showDialog(DIALOG_ACTION_SELECT);
        registerReceiver(mTimerUpdateReceiver, new IntentFilter(BpPanicHandlerService.ACTION_TIMER_UPDATE));
    }

    @Override
    protected void onPause() {
        unregisterReceiver(mTimerUpdateReceiver);
        super.onPause();
    }

    private void doServiceAction(String action) {
        Intent intent = new Intent(this, BpPanicHandlerService.class);
        intent.setAction(action);
        startService(intent);
    }

    public void onClick(DialogInterface dialog, int which) {
        String action;

        switch (which) {
            case DialogInterface.BUTTON_POSITIVE:
                action = BpPanicHandlerService.ACTION_REBOOT;
                break;
            case DialogInterface.BUTTON_NEUTRAL:
                action = BpPanicHandlerService.ACTION_SNOOZE;
                break;
            case DialogInterface.BUTTON_NEGATIVE:
                action = BpPanicHandlerService.ACTION_DISMISS;
                break;
            default:
                return;
        }

        doServiceAction(action);
        finish();
    }

    private void updateDialogMessage(int timeout) {
        StringBuilder message = new StringBuilder();
        message.append(getString(R.string.modem_error_msg));
        if (timeout >= 0) {
            message.append("\n\n");
            message.append(getString(R.string.reboot_timeout_msg, timeout));
        }
        mDialog.setMessage(message.toString());
    }
}
