CC = gcc
CFLAGS =-I. -Iconfig -ggdb -Wall -Werror
LDFLAGS = -Lconfig -lsimpleconfig

TARGET = ircbot

OBJECTS = main.o irc.o socket.o acl.o process.o nope.o

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean: clean-obj clean-bin clean-config

clean-obj:
	rm -rf *.o
	
clean-bin:
	rm -rf $(TARGET)

clean-config:
	make -C config clean
	
$(TARGET): $(OBJECTS) config/libsimpleconfig.a
	$(CC) -g -o $(TARGET) $(OBJECTS) $(LDFLAGS)

config/libsimpleconfig.a:
	make -C config libsimpleconfig.a
