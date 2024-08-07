RACK_DIR ?= ../..

FLAGS +=
CFLAGS +=
CXXFLAGS += -Iexternal/OpenSimplexNoise

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard external/OpenSimplexNoise/OpenSimplexNoise/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

include $(RACK_DIR)/plugin.mk
