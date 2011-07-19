#
# Copyright (C) 2008 The Android Open Source Project
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
#

# This file is executed by build/envsetup.sh, and can use anything
# defined in envsetup.sh.
#
# In particular, you can add lunch options with the add_lunch_combo
# function: add_lunch_combo generic-eng

if [ "${TARGET_PRODUCT}" = "cyanogen_jordan" ]; then

echo Product found : ${TARGET_PRODUCT}, setting custom env.

# faster build
export USE_CCACHE=1

# default build
export CYANOGEN_NIGHTLY=true

# this type of build include kernel and dont backup modules & settings
#export CYANOGEN_RELEASE=1

# to change output filename
#export CYANOGEN_NIGHTLY_BOOT=179

fi

add_lunch_combo generic_jordan-userdebug

