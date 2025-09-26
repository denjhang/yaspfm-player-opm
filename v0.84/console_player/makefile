TARGET = yasp_test.exe
CC = gcc
CFLAGS = -m64 -Wall -Wextra -O2 -I"../ftdi_driver"
LDFLAGS = -m64 -s -L"../ftdi_driver/amd64"
LIBS = -lftd2xx -lm

# Detect OS and set libs accordingly
UNAME_S := $(shell uname -s)

# Default libs
LIBS = -lftd2xx

# Add OS-specific libraries
ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    # Windows (MinGW/MSYS)
    LIBS += -lwinmm -lavrt -lole32
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
    # Windows (MSYS)
    LIBS += -lwinmm -lavrt -lole32
else
    # POSIX-like (Linux, macOS)
    LIBS += -lpthread
endif

SRCS = \
    main.c spfm.c error.c util.c play.c vgm.c s98.c adpcm.c \
    ym2151.c ym2612.c ym2203.c ym2413.c chiptype.c \
    sn76489.c ay8910.c y8950.c ym3526.c ym3812.c \
    ymf262.c ym2608.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
