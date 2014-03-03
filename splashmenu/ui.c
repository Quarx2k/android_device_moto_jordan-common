/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "minui/minui.h"

#ifndef MAX_ROWS
#define MAX_COLS 96
#define MAX_ROWS 50
#endif

#define MENU_MAX_COLS 50
#define MENU_MAX_ROWS 500

#define CHAR_WIDTH 10
#define CHAR_HEIGHT 18

//void gui_print(const char *fmt, ...);
//void gui_print_overwrite(const char *fmt, ...);

static pthread_mutex_t gUpdateMutex = PTHREAD_MUTEX_INITIALIZER;
static gr_surface gBackgroundIcon[NUM_BACKGROUND_ICONS];
static int gUiInitialized = 0;

static const struct { gr_surface* surface; const char *name; } BITMAPS[] = {
    { &gBackgroundIcon[BACKGROUND_DEFAULT], "background-def" },
  //  { &gBackgroundIcon[BACKGROUND_ALT], "background-safe" },
    { &gBackgroundIcon[BACKGROUND_BLANK], "background-blank" },
/*
#ifdef SPLASH_USE_REBOOT
    { &gBackgroundIcon[BACKGROUND_REBOOT], "background-reboot" },
#endif
*/
    { NULL,                             NULL },
};

static gr_surface gCurrentIcon = NULL;

// Set to 1 when both graphics pages are the same (except for the progress bar)
static int gPagesIdentical = 0;

// Log text overlay, displayed when a magic key is pressed
static char text[MAX_ROWS][MAX_COLS];
static int text_cols = 0, text_rows = 0;
static int text_col = 0, text_row = 0, text_top = 0;
static int show_text = 1;

static char menu[MENU_MAX_ROWS][MENU_MAX_COLS]; //allows menu to hold more than stock default 32 rows
static int show_menu = 0;
static int menu_top = 0, menu_items = 0, menu_sel = 0;
static int menu_show_start = 0; ////line count of where the menu starts to be drawn

// Key event input queue
static pthread_mutex_t key_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t key_queue_cond = PTHREAD_COND_INITIALIZER;
static int key_queue[256], key_queue_len = 0;
static volatile char key_pressed[KEY_MAX + 1];

// Clear the screen and draw the currently selected background icon (if any).
// Should only be called with gUpdateMutex locked.
static void draw_background_locked(gr_surface icon)
{
    gPagesIdentical = 0;
    gr_color(0, 0, 0, 255);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    if (icon) {
        int iconWidth = gr_get_width(icon);
        int iconHeight = gr_get_height(icon);
        int iconX = (gr_fb_width() - iconWidth) / 2;
        int iconY = (gr_fb_height() - iconHeight) / 2;
        gr_blit(icon, 0, 0, iconWidth, iconHeight, iconX, iconY);
    }
}

static void draw_text_line(int row, const char* t) {
  if (t[0] != '\0') {
    gr_text(0, row*CHAR_HEIGHT+1, t);
  }
}

// Redraw everything on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
static void draw_screen_locked(void)
{
    draw_background_locked(gCurrentIcon);
    if (show_text) {
        gr_color(0, 0, 0, 160);
        gr_fill(0, 0, gr_fb_width(), gr_fb_height());

        int i = 0, j = 0;
        int k = menu_top + 1; //counter for bottom horizontal text line location
        if (show_menu) {

            //menu line item selection highlight draws
            gr_color(255, 255, 255, 255);
            gr_fill(0, (menu_top + menu_sel - menu_show_start+1) * CHAR_HEIGHT,
                    gr_fb_width(), CHAR_HEIGHT+1);

            //draw semi-static headers
            for (i = 0; i < menu_top; ++i) {
                gr_color(200, 200, 200, 200);
                draw_text_line(i, menu[i]);
                //LOGI("Semi-static headers internal counter i: %i\n", i);
            }

            //adjust counter for current position of selection and menu display starting point
            if (menu_items - menu_show_start + menu_top >= text_rows){
                j = text_rows - menu_top;
                //LOGI("j = text_rows - mneu_top and j = %i\n", j);
           } else {
                j = menu_items - menu_show_start;
                //LOGI("j = mneu_items - menu_show_start and j = %i\n", j);
           }
            //LOGI("outside draw menu items for loop and i goes until limit. limit-menu_show_start + menu_top + j = %i\n", menu_show_start + menu_top + j);
            //draw menu items dynamically based on current menu starting position, menu selection point and headers
             for (i = menu_show_start + menu_top; i < (menu_show_start + menu_top + j); ++i) {
                //LOGI("inside draw menu items for loop and i = %i\n", i);
                if (i == menu_top + menu_sel) {
                    gr_color(200, 200, 200, 200);
                    //LOGI("draw_text_line -menu_item_when_highlighted_color- at i + 1= %i\n", i+1);
                    draw_text_line(i - menu_show_start +1, menu[i]);
                } else {
                    gr_color(200, 200, 200, 200);
                    //LOGI("draw_text_line -menu_item_color- at i + 1= %i\n", i+1);
                    draw_text_line(i - menu_show_start +1, menu[i]);
                }
                //LOGI("inside draw menu items for loop and k = %i\n", k);
                k++;
            }
        }

        k++; //keep ui_print below menu items display
        gr_color(200, 200, 200, 200); //called by at least ui_print
        for (; k < text_rows; ++k) {
            draw_text_line(k, text[(k+text_top) % text_rows]);
        }
    }
}

// Redraw everything on the screen and flip the screen (make it visible).
// Should only be called with gUpdateMutex locked.
static void update_screen_locked(void)
{
    if (!gUiInitialized)    return;
    draw_screen_locked();
    gr_flip();
}

// Reads input events, handles special hot keys, and adds to the key queue.
static void *input_thread(void *cookie)
{
    int rel_sum = 0;
    int fake_key = 0;
    for (;;) {
        // wait for the next key event
        struct input_event ev;
        do {
            ev_get(&ev, 0);

            if (ev.type == EV_SYN) {
                continue;
            } else if (ev.type == EV_REL) {
                if (ev.code == REL_Y) {
                    // accumulate the up or down motion reported by
                    // the trackball.  When it exceeds a threshold
                    // (positive or negative), fake an up/down
                    // key event.
                    rel_sum += ev.value;

                    if (rel_sum > 3) {
                        fake_key = 1;
                        ev.type = EV_KEY;
                        ev.code = KEY_DOWN;
                        ev.value = 1;
                        rel_sum = 0;
                    } else if (rel_sum < -3) {
                        fake_key = 1;
                        ev.type = EV_KEY;
                        ev.code = KEY_UP;
                        ev.value = 1;
                        rel_sum = 0;
                    }
                }
            } else {
                rel_sum = 0;
            }
        } while (ev.type != EV_KEY || ev.code > KEY_MAX);

        pthread_mutex_lock(&key_queue_mutex);
        if (!fake_key) {
            // our "fake" keys only report a key-down event (no
            // key-up), so don't record them in the key_pressed
            // table.
            key_pressed[ev.code] = ev.value;
        }
        fake_key = 0;
        const int queue_max = sizeof(key_queue) / sizeof(key_queue[0]);
        if (ev.value > 0 && key_queue_len < queue_max) {
            key_queue[key_queue_len++] = ev.code;
            pthread_cond_signal(&key_queue_cond);
        }
        pthread_mutex_unlock(&key_queue_mutex);
    }
    return NULL;
}

void ui_init(void)
{
    gr_init();
    ev_init();

    text_col = text_row = 0;
    text_rows = gr_fb_height() / CHAR_HEIGHT;
    if (text_rows > MAX_ROWS) text_rows = MAX_ROWS;
    text_top = 1;

    text_cols = gr_fb_width() / CHAR_WIDTH;
    if (text_cols > MAX_COLS - 1) text_cols = MAX_COLS - 1;

    int i;
    for (i = 0; BITMAPS[i].name != NULL; ++i) {
        int result = res_create_surface(BITMAPS[i].name, BITMAPS[i].surface);
        if (result < 0) {
            if (result == -2) {
                LOGI("Bitmap %s missing header\n", BITMAPS[i].name);
            } else {
                LOGE("Missing bitmap %s\n(Code %d)\n", BITMAPS[i].name, result);
            }
            *BITMAPS[i].surface = NULL;
        }
    }

    pthread_t t;
    pthread_create(&t, NULL, input_thread, NULL);

    gUiInitialized = 1;
}

void ui_final(void)
{
//    evt_exit();

    ui_show_text(0);
    gr_exit();

    //ui_free_bitmaps();
}

void ui_set_background(int icon)
{
    pthread_mutex_lock(&gUpdateMutex);
    gCurrentIcon = gBackgroundIcon[icon];
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_print(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, 512, fmt, ap);
    va_end(ap);

    fputs(buf, stdout);

//    gui_print("%s", buf);

    // This can get called before ui_init(), so be careful.
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        char *ptr;
        for (ptr = buf; *ptr != '\0'; ++ptr) {
            if (*ptr == '\n' || text_col >= text_cols) {
                text[text_row][text_col] = '\0';
                text_col = 0;
                text_row = (text_row + 1) % text_rows;
                if (text_row == text_top) text_top = (text_top + 1) % text_rows;
            }
            if (*ptr != '\n') text[text_row][text_col++] = *ptr;
        }
        text[text_row][text_col] = '\0';
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_print_overwrite(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, (text_cols - 1), fmt, ap);
    va_end(ap);
    //LOGI("ui_print_overwrite - starting text row %i\n", text_row);
    fputs(buf, stdout);

//    gui_print_overwrite("%s", buf);

    text_col = 0;
    // This can get called before ui_init(), so be careful.
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        char *ptr;
        for (ptr = buf; *ptr != '\0'; ++ptr) {
            if (*ptr == '\n' || text_col >= text_cols) {
                text[text_row][text_col] = '\0';
                text_col = 0;
                text_row = (text_row + 1) % text_rows;
                if (text_row == text_top) text_top = (text_top + 1) % text_rows;
            }
            if (*ptr != '\n') text[text_row][text_col++] = *ptr;
        }
        text[text_row][text_col] = '\0';
        // had to comment out as it was being thrown into the output
		//LOGI("ui_print_overwrite - ending text row %i    ending text col%i\n", text_row, text_col); 
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_text_visible()
{
    pthread_mutex_lock(&gUpdateMutex);
    int visible = show_text;
    pthread_mutex_unlock(&gUpdateMutex);
    return visible;
}

void ui_show_text(int visible)
{
    pthread_mutex_lock(&gUpdateMutex);
    show_text = visible;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_key_pressed(int key, int skipkey, int disablekey)
{
    // This is a volatile static array, don't bother locking
    if (key_pressed[key] != 0) {
        return key;
    }
    else if (key_pressed[skipkey] != 0) {
        return skipkey;
    }
    else if (disablekey > -1) {
        if (key_pressed[disablekey] != 0) return disablekey;
    }
    return 0;
}

void ui_clear_key_queue() {
    pthread_mutex_lock(&key_queue_mutex);
    key_queue_len = 0;
    pthread_mutex_unlock(&key_queue_mutex);
}
