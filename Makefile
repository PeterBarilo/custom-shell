CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=gnu18
LOGIN = barilo   
SUBMITPATH = ~cs537-1/handin/barilo/p3 

.PHONY: all clean submit

all: wsh wsh-dbg

wsh: wsh.c wsh.h
	$(CC) $(CFLAGS) -O2 -o $@ $^

wsh-dbg: wsh.c wsh.h
	$(CC) $(CFLAGS) -Og -ggdb -o $@ $^

clean:
	rm -f wsh wsh-dbg

submit:
	cp -r ../ $(SUBMITPATH)
	echo "Submitting project..."
