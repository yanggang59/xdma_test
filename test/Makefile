OUTPUTDIR = ../
CC := gcc
 
 
SRC = $(wildcard *.c)
OBJ = $(patsubst %.c,$(OUTPUTDIR)/%.o,$(notdir $(SRC)))
TARGET = $(patsubst %.c,$(OUTPUTDIR)/%,$(notdir $(SRC)))
 
$(TARGET):$(SRC)
	$(CC) $< -o $@
clean:
	rm -f $(TARGET)