### WHERE #################################
INC_DIR      = ../../../include
CRT0       = ../../../lib/crt0-ia32.o
LIB_DIR    = ../../../lib
CMDBIN_DIR = ../../../rootfs/bin

####################################################################
# this should be the very first rule
_default: all

####################################################################

INCLUDES	+= $(INC_DIR) 
		   
CPPFLAGS	+= -nostdinc -fno-builtin \
	$(addprefix -I, $(INCLUDES)) $(addprefix -D, $(DEFINES))

CFLAGS	+= -Wall -Wno-format -O -fomit-frame-pointer -freg-struct-return
CFLAGS	+= -fsigned-char -funsigned-bitfields


####################################################################
# some evil magic

ifeq ($(findstring -fomit-frame-pointer, $(CFLAGS)), -fomit-frame-pointer)
DEFINES	+= NO_FRAME_POINTER
endif	

####################################################################
# compile rules
include ../../Maketool

#### Linker ########################################################

LINKBASE = 0x00c00000

LIBGCC	 = $(dir $(shell $(CC) --print-lib))
LDFLAGS  += -nostdlib
LDFLAGS  += -L$(LIBGCC) -L$(LIB_DIR)


####################################################################
clean::	
	$(RM) -f *~ *.s *.o *.i *.ii *.s $(LIB)

#########################################

TARGET  =  riotest

OBJS =   riotest.o

all:     $(TARGET)


$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -e_start -Ttext=$(LINKBASE)  -o $@ \
	$(CRT0) $^  -ldraw -lframe -l9p -lbio -lc -lgcc -ll4  -ll4com -ldbg -ll4io 
	cp  $(TARGET)  $(CMDBIN_DIR)

riotest.o: riotest.c

install:  $(TARGET)
	cp  $(TARGET)  $(CMDBIN_DIR)

clean::
	rm -f *~ *.o $(TARGET) 

