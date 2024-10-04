OBJ  = alias_with_mmap.o
EXE = alias_with_mmap
CFLAGS = -O2 
CC = gcc

all: $(OBJ)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJ) -o $(EXE)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf $(EXE) $(OBJ)
