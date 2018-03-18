# Copyright 2016 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
    $(LOCAL_DIR)/dwc3.c \
    $(LOCAL_DIR)/dwc3-commands.c \
    $(LOCAL_DIR)/dwc3-endpoints.c \
    $(LOCAL_DIR)/dwc3-ep0.c \
    $(LOCAL_DIR)/dwc3-events.c

include make/module.mk
