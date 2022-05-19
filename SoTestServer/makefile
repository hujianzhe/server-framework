SOURCE_C_FILE += $(shell find . -name "*.c")
SOURCE_CPP_FILE += $(shell find . -name "*.cpp" && find . -name "*.cc")
LIB_FILE += $(shell find -name "*.a" && find -name "*.so")
TARGET_PATH += .

COMPILE_OPTION := -Wno-deprecated -Wno-parentheses #-fsanitize=address
MACRO := -D_REENTRANT
#ASAN := -fsanitize=address
LIB_PATH := $(PWD)/..
LIB_FILE := $(LIB_PATH)/BootServer/libBootServerDynamic.so
DEBUG_LIB_FILE := $(LIB_PATH)/BootServer/libBootServerDynamicDebug.so
INCLUDE_PATH :=

DEFAULT_LINK := -pthread
#DEFAULT_LINK := -lpthread -lm -ldl
#ifeq ($(shell uname), Linux)
#DEFAULT_LINK += -lrt
#endif

COMPILER := gcc
ifeq ($(COMPILER), gcc)
SOURCE_CPP_FILE :=
endif

TARGET := $(TARGET_PATH)/SoTestServer.exe

all:

debug:
	$(COMPILER) $(MACRO) -D_DEBUG -g $(ASAN) $(COMPILE_OPTION) $(INCLUDE_PATH) $(SOURCE_C_FILE) $(SOURCE_CPP_FILE) $(DEBUG_LIB_FILE) -o $(TARGET) $(DEFAULT_LINK)

release:
	$(COMPILER) $(MACRO) -DNDEBUG -O1 $(COMPILE_OPTION) $(INCLUDE_PATH) $(SOURCE_C_FILE) $(SOURCE_CPP_FILE) $(LIB_FILE) -o $(TARGET) $(DEFAULT_LINK)
