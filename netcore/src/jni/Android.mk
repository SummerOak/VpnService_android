LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := vpncore

#compile with c++ force
LOCAL_CFLAGS := -std=c++11 -DDEBUG

NDK_APP_DST_DIR := ../../libs/$(TARGET_ARCH_ABI)

#specify where to find header files.
LOCAL_C_INCLUDES := $(LOCAL_PATH)/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/core
LOCAL_C_INCLUDES += $(LOCAL_PATH)/core/event
LOCAL_C_INCLUDES += $(LOCAL_PATH)/core/proxy
LOCAL_C_INCLUDES += $(LOCAL_PATH)/core/cipher
LOCAL_C_INCLUDES += $(LOCAL_PATH)/utils
LOCAL_C_INCLUDES += $(LOCAL_PATH)/settings
LOCAL_C_INCLUDES += $(LOCAL_PATH)/reactor
LOCAL_C_INCLUDES += $(LOCAL_PATH)/traffic
LOCAL_C_INCLUDES += $(LOCAL_PATH)/bridge


#specify source file need be compiled.
MY_SRC_LIST := $(wildcard $(LOCAL_PATH)/*.cpp)
MY_SRC_LIST += $(wildcard $(LOCAL_PATH)/core/*.cpp)
MY_SRC_LIST += $(wildcard $(LOCAL_PATH)/core/event/*.cpp)
MY_SRC_LIST += $(wildcard $(LOCAL_PATH)/core/proxy/*.cpp)
MY_SRC_LIST += $(wildcard $(LOCAL_PATH)/core/cipher/*.cpp)
MY_SRC_LIST += $(wildcard $(LOCAL_PATH)/utils/*.cpp)
MY_SRC_LIST += $(wildcard $(LOCAL_PATH)/settings/*.cpp)
MY_SRC_LIST += $(wildcard $(LOCAL_PATH)/reactor/*.cpp)
MY_SRC_LIST += $(wildcard $(LOCAL_PATH)/traffic/*.cpp)
MY_SRC_LIST += $(wildcard $(LOCAL_PATH)/bridge/*.cpp)


LOCAL_SRC_FILES := $(MY_SRC_LIST:$(LOCAL_PATH)/%=%)

#$(warning $(MY_SRC_LIST))

LOCAL_LDLIBS += -llog -ldl -landroid
include $(BUILD_SHARED_LIBRARY)	