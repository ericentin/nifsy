MIX = mix
CFLAGS = -g -O3 -pedantic -Wall -Wextra -Wno-unused-parameter -Wno-gnu

ERLANG_PATH = $(shell erl -eval 'io:format("~s", [lists:concat([code:root_dir(), "/erts-", erlang:system_info(version), "/include"])])' -s init stop -noshell)
CFLAGS += -I$(ERLANG_PATH)

ifneq ($(OS),Windows_NT)
	CFLAGS += -fPIC

	ifeq ($(shell uname),Darwin)
		LDFLAGS += -dynamiclib -undefined dynamic_lookup
	endif
endif

.PHONY: all clean

all: priv/nifsy.so

priv/nifsy.so: c_src/nifsy.c
	mkdir -p priv
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ c_src/nifsy.c

clean:
	$(RM) priv/nifsy.so
	$(RM) -rf priv/nifsy.so.dSYM
