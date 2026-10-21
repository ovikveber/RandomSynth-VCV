RACK_DIR ?= ../Rack-SDK

SOURCES += $(wildcard src/*.cpp)
FLAGS += -std=c++17

include $(RACK_DIR)/plugin.mk
