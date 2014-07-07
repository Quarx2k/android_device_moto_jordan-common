/*
 * Copyright (C) 2007-2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>

#include "common.h"
#include "minui/minui.h"

#ifdef SPLASH_USE_REBOOT
#include <sys/reboot.h>
#ifdef ANDROID_RB_POWEROFF
	#include "cutils/android_reboot.h"
#endif
#endif

static unsigned long total_delay = 8;

/**
 * compare_string()
 *
 */
static int compare_string(const void* a, const void* b) {
  return strcmp(*(const char**)a, *(const char**)b);
}

/**
 * ui_finish()
 *
 */
static void ui_finish(void) {
  LOGI("Exiting....\n");
  ui_final();
}

/**
 * wait_key()
 *
 */
static int wait_key(int key, int skipkey, int disablekey) {
  int i;
  int result = 0;
  int keyp = 0;
  float delay_counter = 0;

//  evt_init();
  for(i=0; i < 400; i++) {
    keyp = ui_key_pressed(key, skipkey, disablekey);
    if(keyp != 0) {
      result = keyp;
      break;
    }
    else {
      usleep(20000);
      delay_counter += .02;
      if (delay_counter >= total_delay)
        break;
    }
  }
//  evt_exit();
  return result;
}


int main(int argc, char **argv) {
  int defmode, mode;
  int result = 1;

  LOGI("Starting Safestrap Splash\n");

  ui_init();
/*
  if (argc >= 2 && 0 == strcmp(argv[1], "1")) {
      ui_set_background(BACKGROUND_ALT);
#ifdef SPLASH_USE_REBOOT
  } else if (argc == 3 && 0 == strcmp(argv[1], "reboot")) {
      ui_set_background(BACKGROUND_REBOOT);
      sleep(2);
      ui_finish();
      LOGI("reboot-command: %s\n", argv[2]);
      if (0 == strcmp(argv[2], "poweroff")) {
#ifdef ANDROID_RB_POWEROFF
        android_reboot(ANDROID_RB_POWEROFF, 0, 0);
#endif
        result = reboot(RB_POWER_OFF);
      } else if (0 == strcmp(argv[2], "recovery")) {
        result = __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, (void*) "recovery");
      } else if (0 == strcmp(argv[2], "bootloader")) {
        result = __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, (void*) "bootloader");
      } else if (0 == strcmp(argv[2], "download")) {
	result = __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, (void*) "download");
      } else {
        result = reboot(RB_AUTOBOOT);
      }
      LOGI("result: %d\n", result);
      exit(result);
#endif
*/
  if (argc >= 2 && 0 == strcmp(argv[1], "2")) {
      ui_set_background(BACKGROUND_BLANK);
  } else if (argc >= 2 && 0 == strcmp(argv[1], "3")) {
      ui_set_background(BACKGROUND_BLANK);
      gr_fb_blank(1);
  } else {
      ui_set_background(BACKGROUND_DEFAULT);
  }

  // HASH: if a 3rd param is passed then use it for the total # of seconds to wait
  if (argc == 3) {
    long int temp = 8;
    char * pEnd;
    temp = strtol(argv[2], &pEnd, 10);
    if (temp > 0)
      total_delay = (float)temp;
  }

  ui_show_text(1);

  int keyp = wait_key(SPLASH_RECOVERY_KEY, SPLASH_CONTINUE_KEY, SPLASH_DISABLE_KEY);
  if (keyp == SPLASH_RECOVERY_KEY)
    result = 0;
  else if (keyp == SPLASH_CONTINUE_KEY)
    result = 1;
  else if (keyp == SPLASH_DISABLE_KEY)
    result = 2;

  ui_set_background(BACKGROUND_BLANK);
  ui_finish();

  sync();
  LOGI("result: %d\n", result);
  exit(result);
}

