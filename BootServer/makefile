SOURCE_C_FILE += $(shell find . -name "*.c")
SOURCE_CPP_FILE += $(shell find . -name "*.cpp" && find . -name "*.cc")
PROTOBUF_FILE += $(shell find -name "*.pb.*")
OBJECT_FILE += $(shell find -name "*.o" && find -name "*.gch")
LIB_FILE += $(shell find -name "*.a" && find -name "*.so")
TARGET_PATH += .

COMPILE_OPTION := -Wno-deprecated -Wno-parentheses
MACRO := -D_REENTRANT
#DLL_PATH := -Wl,-E,-rpath,./:$(TARGET_PATH)

DEFAULT_LINK := -lpthread -lm -ldl
ifeq ($(shell uname), Linux)
DEFAULT_LINK += -lrt
DLL_PATH := -Wl,-E,-rpath,./:$(TARGET_PATH)
else
DLL_PATH := -Wl,-rpath,./:$(TARGET_PATH)
endif

ifeq ($(SOURCE_CPP_FILE), "")
COMPILER := g++
else
COMPILER := gcc
endif

TARGET := $(TARGET_PATH)/BootServer.exe

all:

debug:
	$(COMPILER) $(COMPILE_OPTION) -D_DEBUG -g $(MACRO) $(SOURCE_C_FILE) $(SOURCE_CPP_FILE) -o $(TARGET) $(DLL_PATH) $(DEFAULT_LINK)

release:
	$(COMPILER) $(COMPILE_OPTION) -DNDEBUG $(MACRO) $(SOURCE_C_FILE) $(SOURCE_CPP_FILE) -o $(TARGET) $(DLL_PATH) $(DEFAULT_LINK)
