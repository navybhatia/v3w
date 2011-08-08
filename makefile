CC=gcc
CC_OPT = -Wall -g

V3W_OBJ = v3w.o g726.o g711.o

all:: v3w

clean:
	$(RM) *.o
	$(RM) v3w

.c.o:
	$(CC) $(CC_OPT) -c $<

v3w: $(V3W_OBJ)
	$(CC) -o v3w $(V3W_OBJ) -lm

g726.o:	g726.c
	$(CC) -c $(CC_OPT) g726.c

g711.o:	g711.c
	$(CC) -c $(CC_OPT) g711.c

v3w.o: v3w.c
	$(CC) -c $(CC_OPT) v3w.c

