.PHONY: all clean

all:
	$(MAKE) -C c_src

clean:
	$(MAKE) -C c_src clean
