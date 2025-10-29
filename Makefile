# Minimal Makefile for the tiny derivative-based regex demo

# Configurable variables
CC      ?= cc
CFLAGS  ?= -O2 -g -Wall -Wextra -std=c99 -pedantic
LDFLAGS ?=
LDLIBS  ?=

SRC_DIR := src
BUILD   ?= build
TARGET  ?= derivex

SRCS := $(SRC_DIR)/regex.c $(SRC_DIR)/main.c
OBJS := $(BUILD)/regex.o $(BUILD)/main.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/regex.h | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)





clean:
	rm -rf $(BUILD) $(TARGET)
