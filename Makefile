# Makefile for ls project
# CC     = which compiler to use
# CFLAGS = compiler options (-Wall shows all warnings)
CC = gcc
CFLAGS = -Wall

SRC = src/ls-v1.0.0.c
TARGET = bin/ls

# 'make' runs the first target: build bin/ls
all: $(TARGET)

# bin/ls depends on the source file; rebuild it when the .c file changes
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# 'make clean' deletes the compiled program
clean:
	rm -f $(TARGET)
