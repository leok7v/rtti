CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -Werror -g

I = -Iinclude -Icodable -Illm

all: bin/llms include/models.rtti.h include/completion.rtti.h

bin:
	mkdir -p bin

bin/rtti: rtti/rtti.c | bin
	$(CC) $(CFLAGS) -o $@ rtti/rtti.c

include/%.rtti.h: include/%.h bin/rtti
	bin/rtti $< -o $@

bin/llms: llm/llms.c llm/server.c llm/sb.c \
	codable/codable.c include/models.rtti.h \
	include/completion.rtti.h | bin
	$(CC) $(CFLAGS) $(I) -o $@ llm/llms.c llm/server.c llm/sb.c \
	codable/codable.c

test: all
	bin/llms --all

clean:
	rm -rf bin include/*.rtti.h
