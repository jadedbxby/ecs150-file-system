#Target library
lib := libfs.a

all: $(lib)

## TODO: Phase 1

#Compilation toolchain 
CC := gcc

#Flags 
CFLAGS := -Wall -Wextra -Werror
CFLAGS += -g 

#Object files 
objs := fs.o disk.o 

#Include dependencies
deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

#General rule for libfs.a 
libfs.a: $(objs)
	ar rcs libfs.a $(objs)

#Compile objects 
%.o: %.c 
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< 

#Cleaning rule
clean: 
	$(Q)rm -f $(targets) $(objs) $(deps) 
