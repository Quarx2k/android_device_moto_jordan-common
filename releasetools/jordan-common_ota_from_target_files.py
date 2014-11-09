def InstallEnd_SetSpecificDeviceConfigs(self, *args, **kwargs):
  # Fix Scripts permissions
  self.script.Mount("/bootstrap")
  self.script.SetPermissionsRecursive("/bootstrap/config", 0, 0, 0755, 0664, None, None)
  self.script.SetPermissionsRecursive("/bootstrap/binary", 0, 0, 0755, 0755, None, None)
  self.script.SetPermissionsRecursive("/bootstrap/script", 0, 0, 0755, 0755, None, None)
  self.script.UnpackPackageDir("system/bootstrap", "/bootstrap/bootstrap");
  self.script.AppendExtra('package_extract_file("system/bootstrap/binary/logwrapper", "/bootstrap/bin/logwrapper");')
  self.script.SetPermissions("/bootstrap/bin/logwrapper", 0, 0, 0755, None, None)
  self.script.Unmount("/bootstrap");

def FullOTA_InstallBegin(self, *args, **kwargs):
  self.script.AppendExtra('ifelse((run_program("/sbin/busybox", "ls", "/dev/block/mmcblk1p21") == "0"),  abort("Please resize partition before install CM12.0!"), ui_print("Compatible bootstrap! We can continue!"));')
  self.script.Print("Start install CM-11.0...")

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

  self.script.MakeSymlinks(symlinks)
  self.script.ShowProgress(0.2, 0)

  self.script.Print("Finished installing Lollipop for OMAP3 devices, Enjoy!")

def FullOTA_DisableBootImageInstallation(self, *args, **kwargs):
  return True

def FullOTA_DisableRecoveryUpdate(self, *args, **kwargs):
  return True

def FullOTA_FormatSystemPartition(self, *args, **kwargs):
  self.script.Mount("/system")
  self.script.AppendExtra('delete_recursive("/system");')

  # returning true skips formatting /system!
  return True

def IncrementalOTA_InstallEnd(self, *args, **kwargs):
  InstallEnd_SetSpecificDeviceConfigs(self, args, kwargs)
