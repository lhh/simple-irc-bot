CC = gcc
CFLAGS =-I. -Iconfig -ggdb -Wall -Werror
LDFLAGS = -Lconfig -lsimpleconfig

TARGET = ircbot

OBJECTS = main.o irc.o socket.o acl.o process.o nope.o command.o version.o

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

version.c:
	@RELEASE=$$(git log -1 | awk '/^commit/ { print $$2 }') ;\
	echo "char *_version_ = \"$$RELEASE built $$(date)\";" > version.c

clean: clean-obj clean-bin clean-config

clean-obj:
	rm -rf *.o version.c
	
clean-bin:
	rm -rf $(TARGET)

clean-config:
	make -C config clean
	
$(TARGET): $(OBJECTS) config/libsimpleconfig.a
	$(CC) -g -o $(TARGET) $(OBJECTS) $(LDFLAGS)

config/libsimpleconfig.a:
	make -C config libsimpleconfig.a
