LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_DEPS := \
    lib/cksum

MODULE_CPPFLAGS := -std=c++11
MODULE_CPPFLAGS += -Wno-invalid-offsetof # XXX this may be a terrible idea

MODULE_SRCS += \
	$(LOCAL_DIR)/app.c \
	$(LOCAL_DIR)/channel.cpp \
	$(LOCAL_DIR)/mocom.cpp \
	$(LOCAL_DIR)/mux.cpp \
	$(LOCAL_DIR)/usb.cpp \

include make/module.mk
