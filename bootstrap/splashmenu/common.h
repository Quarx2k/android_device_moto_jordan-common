/*
 * Copyright (C) 2007-2012 The Android Open Source Project
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

#ifndef RECOVERY_COMMON_H
#define RECOVERY_COMMON_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the graphics system.
void ui_init();
void ui_final();

void evt_init();
void evt_exit();

// Use KEY_* codes from <linux/input.h> or KEY_DREAM_* from "minui/minui.h code
int ui_key_pressed(int recovery_key, int boot_key, int adb_key, int uart_key);  // returns >0 if the code is currently pressed
int ui_text_visible();        // returns >0 if text log is currently visible
void ui_show_text(int visible);
void ui_clear_key_queue();

// Write a message to the on-screen log shown with Alt-L (also to stderr).
// The screen is small, and users may need to report these messages to support,
// so keep the output short and not too cryptic.
void ui_print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// same without args
void ui_print_str(char *str);

// Set the icon (normally the only thing visible besides the progress bar).
enum {
  BACKGROUND_ICON_NONE,
  BACKGROUND_DEFAULT,
  BACKGROUND_ALT,
  BACKGROUND_BLANK,
#ifdef SPLASH_USE_REBOOT
  BACKGROUND_REBOOT,
#endif
  NUM_BACKGROUND_ICONS
};
void ui_set_background(int icon);

#define LOGE(...) ui_print("E:" __VA_ARGS__)
#define LOGW(...) fprintf(stdout, "W:" __VA_ARGS__)
#define LOGI(...) fprintf(stdout, "I:" __VA_ARGS__)

#if 1
#define LOGV(...) fprintf(stdout, "V:" __VA_ARGS__)
#define LOGD(...) fprintf(stdout, "D:" __VA_ARGS__)
#else
#define LOGV(...) do {} while (0)
#define LOGD(...) do {} while (0)
#endif

#define STRINGIFY(x) #x
#define EXPAND(x) STRINGIFY(x)

static void finish(void);

enum { 
  DISABLE,
  ENABLE
};

int  ui_create_bitmaps();
void ui_free_bitmaps();

//checkup
int checkup_report(void);

#ifdef __cplusplus
}
#endif

#endif  // RECOVERY_COMMON_H
