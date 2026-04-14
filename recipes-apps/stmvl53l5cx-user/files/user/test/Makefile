CXX ?= g++
CXXFLAGS ?= -Wall -Werror -Wno-missing-braces -Os -g0
CXXFLAGS += -DSTMVL53L5CX_KERNEL
LDFLAGS ?=

CORE_INCLUDE_PATHS = -I../uld-driver/inc
PLATFORM_INCLUDE_PATHS = -I../platform
EXAMPLES_INCLUDE_PATHS = -I../examples

INCLUDE_PATH = $(CORE_INCLUDE_PATHS) $(PLATFORM_INCLUDE_PATHS) $(EXAMPLES_INCLUDE_PATHS)
CXXFLAGS += $(INCLUDE_PATH)

LIB_SOURCES = \
    $(wildcard ../uld-driver/src/*.cpp) \
    $(wildcard ../platform/*.cpp) \
    $(wildcard ../examples/*.c)

all:
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o menu ./menu.c $(LIB_SOURCES)

clean:
	rm -f menu
