CC ?= gcc
.PHONY: all clean

SRCS = gbafix.c
CFLAGS += $(shell pkg-config --cflags libpng)
LDFLAGS += $(shell pkg-config --libs-only-L libpng)

ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

all: gbafix$(EXE)
	@:

gbafix$(EXE): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

clean:
	$(RM) gbafix gbafix.exe
