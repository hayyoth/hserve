CC = cc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -O2

TARGET = hserve
BUILD_DIR = build

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DESTDIR =

SOURCES = main.c server.c socket.c http.c file.c secure.c worker.c
OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)
PROGRAM = $(BUILD_DIR)/$(TARGET)

.PHONY: all clean install uninstall

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

install: $(PROGRAM)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROGRAM) $(DESTDIR)$(BINDIR)/$(TARGET)
	chmod 755 $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
