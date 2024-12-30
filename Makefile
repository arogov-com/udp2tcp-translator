PROJECT := udp2tcp
SOURCE := udp2tcp.c
TEST := test
CC := gcc
CFLAGS := -Wall -O3

default: $(PROJECT)

$(PROJECT): $(SOURCE) Makefile
	$(CC) $(CFLAGS) -o $(PROJECT) $(SOURCE)

clean:
	rm $(PROJECT)
	rm $(TEST)

test: $(TEST).c Makefile
	$(CC) $(CFLAGS) -o $(TEST) $(TEST).c
