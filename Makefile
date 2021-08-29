IDIR = include
SDIR = src
ODIR = obj
CC=gcc
CFLAGS=-I$(IDIR) -lpthread -Wall -Werror -Wextra -g -Wno-format-truncation -lssl -lcrypto
PREFIX = /usr/local/bin

_OBJS=boundless.o chat.o communication.o config.o linkedlist.o logging.o commands.o user.o channel.o security.o events.o group.o modes.o cluster.o hstring.o auth.o ssl.o array.o
OBJS=$(patsubst %,$(ODIR)/%,$(_OBJS))

$(ODIR)/%.o: $(SDIR)/%.c $(IDIR)/%.h
	mkdir -p $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS)

boundlessd: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

install: boundlessd
	mkdir -p ${DESTDIR}${PREFIX}
	cp -f boundlessd ${DESTDIR}${PREFIX}
	chmod 755 ${DESTDIR}${PREFIX}/boundlessd

uninstall:
	rm -f ${DESTDIR}${PREFIX}/boundlessd

clean: 
	rm -f obj/*.o boundlessd
