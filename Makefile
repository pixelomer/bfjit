CC ?= clang
DEBUG ?= 0
LDFLAGS =
CFLAGS = -include macros.h -DDEBUG=$(DEBUG) -Wall -Werror
ifeq ($(shell uname),Darwin)
LDFLAGS += -lSystem
endif
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:%.c=out/%.o)

out/bfjit: out $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o out/bfjit

out/%.o: %.c
	$(CC) $(CFLAGS) -c "$<" -o "$@"

out:
	@mkdir -p out

clean:
	rm -rvf out