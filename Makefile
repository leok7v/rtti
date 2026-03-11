CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -Werror -g

all: Build/rtti Build/client

Build:
	mkdir -p Build

Build/rtti: rtti.c | Build
	TMPDIR=/tmp $(CC) $(CFLAGS) -o $@ rtti.c

Build/client.rtti.h: client.h Build/rtti | Build
	./Build/rtti client.h > $@

Build/models.rtti.h: models.h Build/rtti | Build
	./Build/rtti models.h > $@

Build/client: client.c codable.c server.c Build/client.rtti.h Build/models.rtti.h | Build
	TMPDIR=/tmp $(CC) $(CFLAGS) -I . -I Build -o $@ \
		client.c codable.c server.c

clean:
	rm -f Build/* tmp/* || true

test: all
	mkdir -p tmp
	TMPDIR=/tmp ./Build/client
