CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -Werror -g

all: Build/rtti Build/client

Build:
	mkdir -p Build

Build/rtti: src/rtti.c | Build
	TMPDIR=/tmp $(CC) $(CFLAGS) -o $@ src/rtti.c

Build/client.rtti.h: client.h Build/rtti | Build
	./Build/rtti client.h > $@

Build/client: client.c src/codable.c llm.c Build/client.rtti.h | Build
	TMPDIR=/tmp $(CC) $(CFLAGS) -I src -I . -I Build -o $@ \
		client.c src/codable.c llm.c

clean:
	rm -rf Build tmp

test: all
	mkdir -p tmp
	TMPDIR=/tmp ./Build/client
