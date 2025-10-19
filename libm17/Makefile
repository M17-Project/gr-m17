INC     = -I ./

TARGET  = libm17.so
VERSION = $(shell grep '#define LIBM17_VERSION' m17.h | awk '{print $$3}' | tr -d '"')

CFLAGS  = $(INC) -fPIC -Wall -Wextra
LDFLAGS = -shared -lm
SRCS    = m17.c $(filter-out unit_tests/unit_tests.c, $(wildcard */*.c))
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

fclean:
	rm -f $(TARGET)

test:
	$(CC) $(CFLAGS) unit_tests/unit_tests.c -o unit_tests/unit_tests -lm -lunity -lm17

testrun:
	./unit_tests/unit_tests

install:
	sudo install $(TARGET) /usr/local/lib
	sudo \cp m17.h /usr/local/include
	sudo chmod 644 /usr/local/include/m17.h

$(TARGET): m17.h $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

.PHONY: all clean fclean test testrun install
