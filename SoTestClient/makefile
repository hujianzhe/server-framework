SOURCE_C_FILE += $(shell find . -name "*.c")
SOURCE_CPP_FILE += $(shell find . -name "*.cpp" && find . -name "*.cc")
LIB_FILE += $(shell find -name "*.a" && find -name "*.so")
TARGET_PATH += .

COMPILE_OPTION := -Wno-deprecated -Wno-parentheses
MACRO := -D_REENTRANT
LIB_FILE := $(PWD)/../BootServer/libBootServerDynamic.so
INCLUDE_PATH :=

#DEFAULT_LINK := -lpthread -lm -ldl
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
	$(COMPILER) $(MACRO) -D_DEBUG -g $(COMPILE_OPTION) $(INCLUDE_PATH) $(SOURCE_C_FILE) $(SOURCE_CPP_FILE) $(LIB_FILE) -o $(TARGET) $(DEFAULT_LINK)

release:
	$(COMPILER) $(MACRO) -DNDEBUG -O1 $(COMPILE_OPTION) $(INCLUDE_PATH) $(SOURCE_C_FILE) $(SOURCE_CPP_FILE) $(LIB_FILE) -o $(TARGET) $(DEFAULT_LINK)
