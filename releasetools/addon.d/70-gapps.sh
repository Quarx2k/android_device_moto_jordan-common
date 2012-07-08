#!/sbin/sh
# 
# /system/addon.d/70-gapps.sh
#
. /tmp/backuptool.functions

list_files() {
cat <<EOF
app/ChromeBookmarksSyncAdapter.apk
app/GalleryGoogle.apk app/Gallery2.apk
app/GenieWidget.apk
app/Gmail.apk
app/GoogleBackupTransport.apk
app/GoogleCalendarSyncAdapter.apk
app/GoogleContactsSyncAdapter.apk
app/GoogleFeedback.apk
app/GoogleLoginService.apk
app/GooglePackageVerifier.apk
app/GooglePackageVerifierUpdater.apk
app/GooglePartnerSetup.apk
app/GoogleQuickSearchBox.apk app/QuickSearchBox.apk
app/GoogleServicesFramework.apk
app/GoogleTTS.apk
app/MarketUpdater.apk
app/MediaUploader.apk
app/NetworkLocation.apk
app/OneTimeInitializer.apk
app/SetupWizard.apk app/Provision.apk
app/Talk.apk
app/Vending.apk
app/VoiceSearch.apk
etc/permissions/com.google.android.maps.xml
etc/permissions/com.google.android.media.effects.xml
etc/permissions/com.google.widevine.software.drm.xml
etc/permissions/features.xml
etc/g.prop
framework/com.google.android.maps.jar
framework/com.google.android.media.effects.jar
framework/com.google.widevine.software.drm.jar
lib/libfilterpack_facedetect.so
lib/libflint_engine_jni_api.so
lib/libfrsdk.so
lib/libgcomm_jni.so
lib/libpicowrapper.so
lib/libspeexwrapper.so
lib/libvideochat_jni.so
lib/libvideochat_stabilize.so
lib/libvoicesearch.so
tts/lang_pico/de-DE_gl0_sg.bin
tts/lang_pico/de-DE_ta.bin
tts/lang_pico/es-ES_ta.bin
tts/lang_pico/es-ES_zl0_sg.bin
tts/lang_pico/fr-FR_nk0_sg.bin
tts/lang_pico/fr-FR_ta.bin
tts/lang_pico/it-IT_cm0_sg.bin
tts/lang_pico/it-IT_ta.bin
EOF
}

case "$1" in
  backup)
    list_files | while read FILE DUMMY; do
      backup_file $S/$FILE
    done
  ;;
  restore)
    list_files | while read FILE REPLACEMENT; do
      R=""
      [ -n "$REPLACEMENT" ] && R="$S/$REPLACEMENT"
      [ -f "$C/$S/$FILE" ] && restore_file $S/$FILE $R
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
