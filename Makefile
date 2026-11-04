CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror

.PHONY: all demo test sanitize clean

all: build/riscv

build/riscv: main.c parser.c parser.h rv64i.c rv64i.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) rv64i.c parser.c main.c $(LDFLAGS) -o $@

build/demo: demo.c rv64i.c rv64i.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) rv64i.c demo.c $(LDFLAGS) -o $@

build/test_rv64i: test_rv64i.c rv64i.c rv64i.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) rv64i.c test_rv64i.c $(LDFLAGS) -o $@

demo: build/demo
	./build/demo

build/test_parser: test_parser.c parser.c parser.h rv64i.c rv64i.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) rv64i.c parser.c test_parser.c $(LDFLAGS) -o $@

build/test_decode: test_decode.c rv64i.c rv64i.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) rv64i.c test_decode.c $(LDFLAGS) -o $@

test: build/test_rv64i build/test_parser build/test_decode build/riscv
	./build/test_rv64i
	./build/test_parser
	./build/test_decode
	sh test_cli.sh ./build/riscv

build/test_sanitize: test_rv64i.c rv64i.c rv64i.h
	mkdir -p build
	$(CC) $(CPPFLAGS) -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror \
	    -fsanitize=address,undefined -fno-omit-frame-pointer \
	    rv64i.c test_rv64i.c $(LDFLAGS) -o $@

build/parser_sanitize: test_parser.c parser.c parser.h rv64i.c rv64i.h
	mkdir -p build
	$(CC) $(CPPFLAGS) -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror \
	    -fsanitize=address,undefined -fno-omit-frame-pointer \
	    rv64i.c parser.c test_parser.c $(LDFLAGS) -o $@

build/decode_sanitize: test_decode.c rv64i.c rv64i.h
	mkdir -p build
	$(CC) $(CPPFLAGS) -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror \
	    -fsanitize=address,undefined -fno-omit-frame-pointer \
	    rv64i.c test_decode.c $(LDFLAGS) -o $@

build/riscv_sanitize: main.c parser.c parser.h rv64i.c rv64i.h
	mkdir -p build
	$(CC) $(CPPFLAGS) -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror \
	    -fsanitize=address,undefined -fno-omit-frame-pointer \
	    rv64i.c parser.c main.c $(LDFLAGS) -o $@

sanitize: build/test_sanitize build/parser_sanitize build/decode_sanitize build/riscv_sanitize
	./build/test_sanitize
	./build/parser_sanitize
	./build/decode_sanitize
	sh test_cli.sh ./build/riscv_sanitize

clean:
	rm -f build/demo build/riscv build/test_rv64i build/test_parser build/test_decode \
	    build/test_sanitize build/parser_sanitize build/decode_sanitize build/riscv_sanitize
