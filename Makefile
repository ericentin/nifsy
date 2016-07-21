MIX = mix
CFLAGS = -O3 -pedantic -Weverything -Wall -Wextra -Wno-unused-parameter -Wno-gnu

ERLANG_PATH = $(shell erl -eval 'io:format("~s", [lists:concat([code:root_dir(), "/erts-", erlang:system_info(version), "/include"])])' -s init stop -noshell)
ERLANG_DIRTY_SCHEDULERS = $(shell erl -eval 'io:format("~s", [try erlang:system_info(dirty_cpu_schedulers), true catch _:_ -> false end])' -s init stop -noshell)
CFLAGS += -I$(ERLANG_PATH)

ifndef MIX_ENV
	MIX_ENV = dev
endif

ifneq ($(OS),Windows_NT)
	CFLAGS += -fPIC

	ifeq ($(shell uname),Darwin)
		LDFLAGS += -dynamiclib -undefined dynamic_lookup
	endif
endif

ifeq ($(ERLANG_DIRTY_SCHEDULERS),true)
	CFLAGS += -DERTS_DIRTY_SCHEDULERS
endif

ifdef DEBUG
	CFLAGS += -DNIFSY_DEBUG
endif

ifeq ($(MIX_ENV),dev)
	CFLAGS += -g
endif

.PHONY: all clean

all: priv/$(MIX_ENV)/nifsy.so

priv/$(MIX_ENV)/nifsy.so: c_src/nifsy.c
	mkdir -p priv/$(MIX_ENV)
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ c_src/nifsy.c

clean:
	$(RM) -r priv/$(MIX_ENV)
