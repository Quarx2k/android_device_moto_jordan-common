def InstallEnd_SetSpecificDeviceConfigs(self, *args, **kwargs):
  # Fix Scripts permissions
  self.script.SetPermissionsRecursive("/system/bootstrap/config", 0, 0, 0755, 0664, None, None)
  self.script.SetPermissionsRecursive("/system/bootstrap/binary", 0, 0, 0755, 0755, None, None)
  self.script.SetPermissionsRecursive("/system/bootstrap/script", 0, 0, 0755, 0755, None, None)
  # Install correct Media-Profiles
#  self.script.AppendExtra('package_extract_file("system/bin/camera_detect", "/tmp/camera_detect");')
#  self.script.AppendExtra('set_perm(0, 0, 0777, "/tmp/camera_detect");')
#  self.script.AppendExtra('run_program("/tmp/camera_detect");')
#  self.script.AppendExtra('ifelse(is_substring("mt9p012", getprop("ro.camera.type")), run_program("/sbin/sh", "-c", "busybox mv /system/etc/media_profiles_mb526.xml /system/etc/media_profiles.xml"));')
#  self.script.AppendExtra('ifelse(is_substring("camise", getprop("ro.camera.type")), run_program("/sbin/sh", "-c", "busybox mv /system/etc/media_profiles_mb525.xml /system/etc/media_profiles.xml"));')
#  self.script.AppendExtra('package_extract_file("system/etc/media_profiles_mb526.xml", "/system/etc/media_profiles.xml");')
#  self.script.AppendExtra('delete("/system/etc/media_profiles_mb526.xml");')
 # self.script.AppendExtra('delete("/system/etc/media_profiles_mb525.xml");')


def FullOTA_InstallBegin(self, *args, **kwargs):
  self.script.AppendExtra('run_program("/sbin/tune2fs", "-O has_journal /dev/block/mmcblk1p24");')
  self.script.AppendExtra('run_program("/sbin/tune2fs", "-O has_journal /dev/block/mmcblk1p25");')

def FullOTA_InstallEnd(self, *args, **kwargs):
  self.script.Print("Wiping cache...")
  self.script.Mount("/cache")
  self.script.AppendExtra('delete_recursive("/cache");')
  self.script.Print("Wiping dalvik-cache...")
  self.script.Mount("/data")
  self.script.AppendExtra('delete_recursive("/data/dalvik-cache");')
  self.script.Print("Wiping battd stats...")
  self.script.AppendExtra('delete_recursive("/data/battd");')

# DeviceConfig
  InstallEnd_SetSpecificDeviceConfigs(self, args, kwargs)
  
  self.script.SetPermissionsRecursive("/system/etc/init.d", 0, 0, 0755, 0555, None, None)
  self.script.SetPermissionsRecursive("/system/addon.d", 0, 0, 0755, 0755, None, None)
  self.script.SetPermissions("/system/etc/motorola/comm_drv/commdrv_fs.sh", 0, 0, 0755, None, None)

  symlinks = []

  # libaudio link fix
  symlinks.append(("/system/lib/hw/audio.a2dp.default.so", "/system/lib/liba2dp.so"))
  #symlinks.append(("/system/lib/hw/audio.primary.omap3.so", "/system/lib/libaudio.so"))

  self.script.MakeSymlinks(symlinks)
  self.script.ShowProgress(0.2, 0)

  self.script.Print("Finished installing KitKat for OMAP3 devices, Enjoy!")

def FullOTA_DisableBootImageInstallation(self, *args, **kwargs):
  return True

def FullOTA_FormatSystemPartition(self, *args, **kwargs):
  self.script.Mount("/system")
  self.script.AppendExtra('delete_recursive("/system");')

  # returning true skips formatting /system!
  return True

def IncrementalOTA_DisableRecoveryUpdate(self, *args, **kwargs):
  return True

def IncrementalOTA_InstallEnd(self, *args, **kwargs):
  InstallEnd_SetSpecificDeviceConfigs(self, args, kwargs)
