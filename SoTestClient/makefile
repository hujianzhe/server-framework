SOURCE_C_FILE += $(shell find . -name "*.c")
SOURCE_CPP_FILE += $(shell find . -name "*.cpp" && find . -name "*.cc")
TARGET_PATH += .

COMPILE_OPTION := -Wno-deprecated -Wno-parentheses -Wno-unused-result
MACRO := -D_REENTRANT
LIB_PATH := $(PWD)/..
RELEASE_BOOTSERVER_FILE := $(LIB_PATH)/BootServer/libBootServerDynamic.so
DEBUG_BOOTSERVER_FILE := $(LIB_PATH)/BootServer/libBootServerDynamicDebug.so
ASAN_BOOTSERVER_FILE := $(LIB_PATH)/BootServer/libBootServerDynamicAsan.so
INCLUDE_PATH :=

DEFAULT_LINK := -pthread
#ifeq ($(shell uname), Linux)
#DEFAULT_LINK += -lrt
#endif

COMPILER := gcc
ifeq ($(COMPILER), gcc)
SOURCE_CPP_FILE :=
endif

TARGET := $(TARGET_PATH)/SoTestClient.exe

all:

debug:
	$(COMPILER) $(MACRO) -D_DEBUG -g $(COMPILE_OPTION) $(INCLUDE_PATH) $(SOURCE_C_FILE) $(SOURCE_CPP_FILE) $(DEBUG_BOOTSERVER_FILE) -o $(TARGET) $(DEFAULT_LINK)

asan:
	$(COMPILER) $(MACRO) -D_DEBUG -g -fsanitize=address $(COMPILE_OPTION) $(INCLUDE_PATH) $(SOURCE_C_FILE) $(SOURCE_CPP_FILE) $(ASAN_BOOTSERVER_FILE) -o $(TARGET) $(DEFAULT_LINK)

release:
	$(COMPILER) $(MACRO) -DNDEBUG -O1 $(COMPILE_OPTION) $(INCLUDE_PATH) $(SOURCE_C_FILE) $(SOURCE_CPP_FILE) $(RELEASE_BOOTSERVER_FILE) -o $(TARGET) $(DEFAULT_LINK)
