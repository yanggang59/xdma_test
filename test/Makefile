OUTPUTDIR = ../
CC := gcc
 
 
USR_TEST_SRC = user_debug_test.c
OBJ = $(patsubst %.c,$(OUTPUTDIR)/%.o,$(notdir $(SRC)))
TARGET = read_write_test simple_io_test dump_msg_on dump_msg_off hack_ctl_on hack_ctl_off rnd_rw_test test8K_ramdom hold_ic_ctl_on hold_ic_ctl_off server client

$(TARGET):
	$(CC) $(USR_TEST_SRC) -DREAD_WRITE_TEST -o read_write_test
	$(CC) $(USR_TEST_SRC) -DSIMPLE_IO_TEST -o simple_io_test
	$(CC) $(USR_TEST_SRC) -DDUMP_MSG_ON -o dump_msg_on
	$(CC) $(USR_TEST_SRC) -DDUMP_MSG_OFF -o dump_msg_off
	$(CC) $(USR_TEST_SRC) -DHACK_CTL_ON -o hack_ctl_on
	$(CC) $(USR_TEST_SRC) -DHACK_CTL_OFF -o hack_ctl_off
	$(CC) $(USR_TEST_SRC) -DRND_RW_TEST -o rnd_rw_test
	./gen_data.sh
	$(CC) $(USR_TEST_SRC) -DHOLD_IC_CTL_ON -o hold_ic_ctl_on
	$(CC) $(USR_TEST_SRC) -DHOLD_IC_CTL_OFF -o hold_ic_ctl_off
	$(CC) server.c -o server
	$(CC) client.c -o client
	cp $(TARGET) ../build



clean:
	rm -f $(TARGET)