SOURCE_C_FILE += $(shell find . -name "*.c")
SOURCE_CPP_FILE += $(shell find . -name "*.cpp" && find . -name "*.cc")
PROTOBUF_FILE += $(shell find -name "*.pb.*")
OBJECT_FILE += $(shell find -name "*.o" && find -name "*.gch")
LIB_FILE += $(shell find -name "*.a" && find -name "*.so")
TARGET_PATH += .

COMPILE_OPTION := -Wno-deprecated -Wno-parentheses
MACRO := -D_REENTRANT
DLL_PATH := -Wl,-rpath,./:$(TARGET_PATH)
INCLUDE_PATH :=

DEFAULT_LINK := -lpthread -lm -ldl
ifeq ($(shell uname), Linux)
COMPILE_OPTION += -fPIC -shared
DEFAULT_LINK += -lrt
else ifeq ($(shell uname), Darwin)
INCLUDE_PATH += -I ../BootServer
SOURCE_C_FILE += $(shell find ../BootServer -name "*.c")
SOURCE_CPP_FILE += $(shell find ../BootServer -name "*.cpp" && find ../BootServer -name "*.cc")
MACRO += -DUSE_STATIC_MODULE
endif

ifeq ($(SOURCE_CPP_FILE), "")
COMPILER := g++
else
COMPILER := gcc
endif

ifeq ($(shell uname), Darwin)
TARGET := $(TARGET_PATH)/SoTest.exe
else
TARGET := $(TARGET_PATH)/libSoTest.so
endif

all:

debug:
	$(COMPILER) $(MACRO) -D_DEBUG -g $(COMPILE_OPTION) $(INCLUDE_PATH) $(SOURCE_C_FILE) -o $(TARGET) $(DLL_PATH) $(DEFAULT_LINK)

release:
	$(COMPILER) $(MACRO) -DNDEBUG -O1 $(COMPILE_OPTION) $(INCLUDE_PATH) $(SOURCE_C_FILE) -o $(TARGET) $(DLL_PATH) $(DEFAULT_LINK)
