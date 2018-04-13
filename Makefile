CC = gcc
CFLAGS =-I. -Iconfig -g -Wall -Werror
LDFLAGS = -Lconfig -lsimpleconfig

TARGET = ircbot

OBJECTS = main.o irc.o socket.o

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean: clean-obj clean-bin

clean-obj:
	rm -rf *.o
	
clean-bin:
	rm -rf $(TARGET)
	
$(TARGET): $(OBJECTS)
	$(CC) -g -o $(TARGET) $(OBJECTS) $(LDFLAGS)
