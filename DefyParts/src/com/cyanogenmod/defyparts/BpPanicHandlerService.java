package com.cyanogenmod.defyparts;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.Service;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.content.Intent;
import android.os.IBinder;
import android.os.PowerManager;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.Log;

public class BpPanicHandlerService extends Service {
    private static final String TAG = "BpPanicHandlerService";
    private static final int NOTIFICATION_ID = 1;

    private static final String ACTION_HANDLE_PANIC = "com.cyanogenmod.defyparts.action.HANDLE_BP_PANIC";
    static final String ACTION_TIMER_UPDATE = "com.cyanogenmod.defyparts.action.TIMER_UPDATE";
    static final String ACTION_REBOOT = "com.cyanogenmod.defyparts.action.BP_PANIC_REBOOT";
    static final String ACTION_SNOOZE = "com.cyanogenmod.defyparts.action.REMIND_LATER";
    static final String ACTION_DISMISS = "com.cyanogenmod.defyparts.action.DISMISS";

    static final String EXTRA_TIMEOUT = "timeout";

    static final String KEY_NEED_REBOOT_NOTICE = "need_bppanic_reboot_notice";

    private static final long SNOOZE_DELAY = 5 * 60 * 1000; /* 5 minutes */
    private static final long REBOOT_TIMEOUT = 60 * 1000; /* 1 minute */

    private SharedPreferences mPrefs;
    private AlarmManager mAM;
    private long mRebootTimeout = -1;
    private Notification mNotification;
    private Intent mNotifyIntent;
    private PendingIntent mTimerUpdateIntent;
    private PendingIntent mPanicHandleIntent;

    @Override
    public void onCreate() {
        super.onCreate();

        mAM = (AlarmManager) getSystemService(ALARM_SERVICE);
        mPrefs = PreferenceManager.getDefaultSharedPreferences(this);

        Intent intent = new Intent(this, getClass());
        intent.setAction(ACTION_HANDLE_PANIC);
        mPanicHandleIntent = PendingIntent.getService(this, 0, intent, 0);

        intent = new Intent(this, getClass());
        intent.setAction(ACTION_TIMER_UPDATE);
        mTimerUpdateIntent = PendingIntent.getService(this, 1, intent, 0);

        mNotifyIntent = new Intent(this, BpPanicNotifyActivity.class);
        mNotifyIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent.getAction();

        if (TextUtils.equals(action, ACTION_HANDLE_PANIC)) {
            Log.d(TAG, "Got BP panic intent");
            if (mNotification == null) {
                mNotification = createNotification();
            }
            mRebootTimeout = System.currentTimeMillis() + REBOOT_TIMEOUT;
            updateTimeout();
            startActivity(mNotifyIntent);
            startForeground(NOTIFICATION_ID, mNotification);
        } else if (TextUtils.equals(action, ACTION_REBOOT)) {
            doReboot(false);
        } else if (TextUtils.equals(action, ACTION_SNOOZE)) {
            cancelTimeout();
            schedule(mPanicHandleIntent, SNOOZE_DELAY);
        } else if (TextUtils.equals(action, ACTION_DISMISS)) {
            cancelTimeout();
        } else if (TextUtils.equals(action, ACTION_TIMER_UPDATE)) {
            updateTimeout();
        }

        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void schedule(PendingIntent intent, long timeout) {
        mAM.set(AlarmManager.RTC_WAKEUP, System.currentTimeMillis() + timeout, intent);
    }

    private void cancelTimeout() {
        removeStickyBroadcast(new Intent(ACTION_TIMER_UPDATE));
        mRebootTimeout = -1;
        mAM.cancel(mTimerUpdateIntent);
    }

    private Notification createNotification() {
        Notification notification = new Notification(android.R.drawable.stat_notify_error, null, System.currentTimeMillis());
        notification.defaults |= Notification.DEFAULT_SOUND | Notification.DEFAULT_VIBRATE;
        notification.flags |= Notification.FLAG_ONGOING_EVENT | Notification.FLAG_SHOW_LIGHTS;
        notification.ledARGB = Color.RED;
        notification.ledOnMS = 1000;
        notification.ledOffMS = 0;
        notification.setLatestEventInfo(this,
                getString(R.string.modem_error),
                getString(R.string.touch_for_info),
                PendingIntent.getActivity(this, 0, mNotifyIntent, 0));

        return notification;
    }

    private void updateTimeout() {
        int timeout = (int) (mRebootTimeout - System.currentTimeMillis() + 500) / 1000;
        if (timeout <= 0) {
            doReboot(true);
        } else {
            Intent intent = new Intent(ACTION_TIMER_UPDATE);
            intent.putExtra(EXTRA_TIMEOUT, timeout);
            sendStickyBroadcast(intent);
            schedule(mTimerUpdateIntent, timeout <= 5 ? 1000 : 5000);
        }
    }

    private void doReboot(boolean requireNotice) {
        PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
        mPrefs.edit().putBoolean(KEY_NEED_REBOOT_NOTICE, requireNotice).commit();
        pm.reboot("bppanic");
    }
}
