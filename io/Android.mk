# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)

# Raw memory i/o utility
# http://www.sleepie.demon.co.uk/linuxvme/io.c

# Description:
#  This tool can be used to access physical memory addresses from userspace.
#  It can be useful to access hardware for which no device driver exists!
#
# Forked from OpenWRT io Package (v2.0)
#  Fix pointer arithmetic warnings, and setup for Android Repo

include $(CLEAR_VARS)

LOCAL_SRC_FILES := io.c
LOCAL_MODULE := io
LOCAL_MODULE_TAGS := eng

include $(BUILD_EXECUTABLE)

