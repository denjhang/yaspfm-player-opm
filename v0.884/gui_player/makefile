CC = g++
CFLAGS = -m64 -Wall -Wextra -O2 -I"../ftdi_driver" -fpermissive
LDFLAGS = -m64 -s -L"../ftdi_driver/amd64"
LIBS = -lftd2xx -lwinmm -lgdi32 -mwindows
TARGET = yasp_gui.exe
OBJS = main.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
