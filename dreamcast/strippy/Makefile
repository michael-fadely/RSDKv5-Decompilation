TRISTIPPER_DIR = libs/TriStripper

all: library main

library:
	$(MAKE) -C $(TRISTIPPER_DIR)

main: library
	c++ -O3 main.cpp -o strippy -L$(TRISTIPPER_DIR) -lTriStripper -lm
clean:
	$(MAKE) -C $(TRISTIPPER_DIR) clean
	rm -f strippy

.PHONY: all library main clean
