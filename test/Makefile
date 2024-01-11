OUTPUTDIR = ../
CC := gcc
 
 
SRC = $(wildcard *.c)
OBJ = $(patsubst %.c,$(OUTPUTDIR)/%.o,$(notdir $(SRC)))
TARGET = read_write_test simple_io_test dump_msg_on dump_msg_off

$(TARGET):$(SRC)
	$(CC) $< -DREAD_WRITE_TEST -o read_write_test
	$(CC) $< -DSIMPLE_IO_TEST -o simple_io_test
	$(CC) $< -DDUMP_MSG_ON -o dump_msg_on
	$(CC) $< -DDUMP_MSG_OFF -o dump_msg_off

clean:
	rm -f $(TARGET)