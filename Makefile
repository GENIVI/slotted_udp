#
# Makefile for slotted udp
#

TARGET = slotted_udp_test

OBJ = slotted_udp.o
HDR = slotted_udp.h
CFLAGS = -g -Wall

$(TARGET): $(OBJ) $(TARGET).o
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(TARGET).o

$(OBJ): $(HDR)

clean:
	rm -f $(TARGET) $(OBJ) $(TARGET).o
