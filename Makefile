IDIR = include
SDIR = src
ODIR = obj
CC=gcc
CFLAGS=-I$(IDIR) -lpthread -Wall -Werror -Wextra -g -Wno-format-truncation

_OBJS=boundless.o chat.o communication.o config.o linkedlist.o logging.o commands.o user.o channel.o security.o events.o group.o modes.o cluster.o
OBJS=$(patsubst %,$(ODIR)/%,$(_OBJS))

$(ODIR)/%.o: $(SDIR)/%.c $(IDIR)/%.h
	mkdir -p $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

clean: 
	rm -f obj/*.o server
