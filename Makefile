#
# Makefile for slotted udp
#

TEST_TARGET = slotted_udp_test
MASTER_TARGET = slotted_udp_master

OBJ = slotted_udp.o
HDR = slotted_udp.h
CFLAGS = -g -Wall

all: $(TEST_TARGET) $(MASTER_TARGET)

$(TEST_TARGET): $(OBJ) $(TEST_TARGET).o
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(OBJ) $(TEST_TARGET).o

$(MASTER_TARGET): $(OBJ) $(MASTER_TARGET).o
	$(CC) $(CFLAGS) -o $(MASTER_TARGET) $(OBJ) $(MASTER_TARGET).o

$(OBJ): $(HDR)

clean:
	rm -f  $(OBJ) $(TEST_TARGET).o $(TEST_TARGET) $(MASTER_TARGET).o $(MASTER_TARGET)
