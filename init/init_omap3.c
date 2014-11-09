/*
   Copyright (c) 2013, The Linux Foundation. All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <private/android_filesystem_config.h>

#include "vendor_init.h"
#include "property_service.h"
#include "log.h"
#include "util.h"
#include <sys/system_properties.h>

#include <sys/resource.h>

static int read_file2(const char *fname, char *data, int max_size)
{
    int fd, rc;

    if (max_size < 1)
        return 0;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        ERROR("failed to open '%s'\n", fname);
        return 0;
    }

    rc = read(fd, data, max_size - 1);
    if ((rc > 0) && (rc < max_size))
        data[rc] = '\0';
    else
        data[0] = '\0';
    close(fd);

    return 1;
}


void vendor_load_properties()
{
    int rc;
    char model[16];
    rc = read_file2("/proc/device-tree/Chosen@0/usb_id_prod_name", model, sizeof(model));

    if (rc) {
       if (!strcmp(model, "MB520")) { //Motorola Bravo
                property_set("ro.product.device", "mb520");
                property_set("ro.product.name", "mb520_umts");
                property_set("ro.product.model", "MB520");
                property_set("ro.build.description", "ro.build.description=mb520-user 4.4.4 KTU84Q 20141001 release-keys");
                property_set("ro.build.fingerprint", "ro.build.fingerprint=motorola/mb520_umts/mb520:4.4.4/KTU84Q/20141001:user/release-keys");
                property_set("ro.media.capture.maxres", "3m");
                property_set("ro.media.capture.classification", "classA");
       } else if (!strcmp(model, "MB526")) { //Motorola Defy
                property_set("ro.product.device", "mb526");
                property_set("ro.product.name", "mb526_umts");
                property_set("ro.product.model", "MB526");
                property_set("ro.build.description", "ro.build.description=mb526-user 4.4.4 KTU84Q 20141001 release-keys");
                property_set("ro.build.fingerprint", "ro.build.fingerprint=motorola/mb526_umts/mb526:4.4.4/KTU84Q/20141001:user/release-keys");
                property_set("ro.media.capture.maxres", "5m");
                property_set("ro.media.capture.flash", "led");
                property_set("ro.media.capture.flashIntensity", "41");
                property_set("ro.media.capture.torchIntensity", "25");
                property_set("ro.media.capture.classification", "classE");

       }
    } 
}
