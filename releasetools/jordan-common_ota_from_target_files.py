def InstallEnd_SetBootmenuPermissions(self, *args, **kwargs):
  self.script.SetPermissionsRecursive("/system/bootmenu/config", 0, 0, 0755, 0664, None, None)
  self.script.SetPermissionsRecursive("/system/bootmenu/binary", 0, 0, 0755, 0755, None, None)
  self.script.SetPermissionsRecursive("/system/bootmenu/script", 0, 0, 0755, 0755, None, None)

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

# Bootmenu
  InstallEnd_SetBootmenuPermissions(self, args, kwargs)

  self.script.SetPermissionsRecursive("/system/etc/init.d", 0, 0, 0755, 0555, None, None)
  self.script.SetPermissionsRecursive("/system/addon.d", 0, 0, 0755, 0755, None, None)
  self.script.SetPermissions("/system/etc/motorola/comm_drv/commdrv_fs.sh", 0, 0, 0755, None, None)
  self.script.UnpackPackageDir("system/bin/bootmenu", "/system/bootmenu/binary/bootmenu")

  symlinks = []

  symlinks.append(("/system/bin/bootmenu", "/system/bin/logwrapper"))
  symlinks.append(("indeterminate.png", "/system/bootmenu/images/indeterminate1.png"))
  symlinks.append(("indeterminate.png", "/system/bootmenu/images/indeterminate2.png"))
  symlinks.append(("indeterminate.png", "/system/bootmenu/images/indeterminate3.png"))
  symlinks.append(("indeterminate.png", "/system/bootmenu/images/indeterminate4.png"))
  symlinks.append(("indeterminate.png", "/system/bootmenu/images/indeterminate5.png"))
  symlinks.append(("indeterminate.png", "/system/bootmenu/images/indeterminate6.png"))

  # libaudio link fix
  symlinks.append(("/system/lib/hw/audio.a2dp.default.so", "/system/lib/liba2dp.so"))

  self.script.MakeSymlinks(symlinks)
  self.script.ShowProgress(0.2, 0)

  self.script.Print("Finished installing Jelly Bean for OMAP3 devices, Enjoy!")

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
  InstallEnd_SetBootmenuPermissions(self, args, kwargs)
