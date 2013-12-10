/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef __BLUEDROID_BLUETOOTH_H__
#define __BLUEDROID_BLUETOOTH_H__
#ifdef __cplusplus
extern "C" {
#endif
#include "../bluetooth.h"

/* Enable the bluetooth interface.
 *
 * Responsible for power on, bringing up HCI interface, and starting daemons.
 * Will block until the HCI interface and bluetooth daemons are ready to
 * use.
 *
 * Returns 0 on success, -ve on error */
int bt_enable();

/* Disable the bluetooth interface.
 *
 * Responsbile for stopping daemons, pulling down the HCI interface, and
 * powering down the chip. Will block until power down is complete, and it
 * is safe to immediately call enable().
 *
 * Returns 0 on success, -ve on error */
int bt_disable();

/* Returns 1 if enabled, 0 if disabled, and -ve on error */
int bt_is_enabled();

int ba2str(const bdaddr_t *ba, char *str);
int str2ba(const char *str, bdaddr_t *ba);

#ifdef __cplusplus
}
#endif
#endif //__BLUEDROID_BLUETOOTH_H__
