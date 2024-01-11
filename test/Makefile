OUTPUTDIR = ../
CC := gcc
 
 
SRC = $(wildcard *.c)
OBJ = $(patsubst %.c,$(OUTPUTDIR)/%.o,$(notdir $(SRC)))
TARGET = read_write_test simple_io_test dump_msg_on dump_msg_off hack_ctl_on hack_ctl_off rnd_rw_test test8K_ramdom

$(TARGET):$(SRC)
	$(CC) $< -DREAD_WRITE_TEST -o read_write_test
	$(CC) $< -DSIMPLE_IO_TEST -o simple_io_test
	$(CC) $< -DDUMP_MSG_ON -o dump_msg_on
	$(CC) $< -DDUMP_MSG_OFF -o dump_msg_off
	$(CC) $< -DHACK_CTL_ON -o hack_ctl_on
	$(CC) $< -DHACK_CTL_OFF -o hack_ctl_off
	$(CC) $< -DRND_RW_TEST -o rnd_rw_test
	./gen_data.sh

clean:
	rm -f $(TARGET)