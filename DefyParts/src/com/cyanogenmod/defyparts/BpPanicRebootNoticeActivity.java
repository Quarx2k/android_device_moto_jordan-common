package com.cyanogenmod.defyparts;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

public class BpPanicRebootNoticeActivity extends Activity {
    private static final int DIALOG_NOTICE = 1;

    @Override
    protected Dialog onCreateDialog(int id, Bundle args) {
        if (id == DIALOG_NOTICE) {
            return new AlertDialog.Builder(this)
                    .setTitle(R.string.modem_error)
                    .setMessage(R.string.device_rebooted_modem_error)
                    .setNegativeButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            dialog.cancel();
                        }
                    })
                    .setOnCancelListener(new DialogInterface.OnCancelListener() {
                        @Override
                        public void onCancel(DialogInterface dialog) {
                            finish();
                        }
                    })
                    .create();
        }

        return super.onCreateDialog(id, args);
    }

    @Override
    protected void onResume() {
        super.onResume();
        showDialog(DIALOG_NOTICE);
    }

    @Override
    protected void onPause() {
        dismissDialog(DIALOG_NOTICE);
        super.onPause();
    }
}
