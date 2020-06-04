SOURCE_C_FILE += $(shell find . -name "*.c")
SOURCE_CPP_FILE += $(shell find . -name "*.cpp" && find . -name "*.cc")
PROTOBUF_FILE += $(shell find -name "*.pb.*")
OBJECT_FILE += $(shell find -name "*.o" && find -name "*.gch")
LIB_FILE += $(shell find -name "*.a" && find -name "*.so")
TARGET_PATH += .

COMPILE_OPTION := -Wno-deprecated -Wno-parentheses
MACRO := -D_REENTRANT
DLL_PATH := -Wl,-rpath,./:$(TARGET_PATH)

DEFAULT_LINK := -lpthread -lm -ldl
ifeq ($(shell uname), Linux)
DEFAULT_LINK += -lrt
endif

ifeq ($(SOURCE_CPP_FILE), "")
COMPILER := g++
else
COMPILER := gcc
endif

TARGET := $(TARGET_PATH)/libSoTest.so

all:

debug:
	$(COMPILER) -fPIC -shared $(MACRO) -g $(COMPILE_OPTION) $(SOURCE_C_FILE) -o $(TARGET) $(DLL_PATH) $(DEFAULT_LINK)

release:
	$(COMPILER) -fPIC -shared $(MACRO) $(COMPILE_OPTION) $(SOURCE_C_FILE) -o $(TARGET) $(DLL_PATH) $(DEFAULT_LINK)
