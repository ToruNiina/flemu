all:
	g++-10 -std=c++20 -Wall -Wextra -Wpedantic -Wfatal-errors -I../extlib/ut/include -I../include test.cpp -DFLEMU_ACTIVATE_UNIT_TESTS -o test

.PHONY:test
test:
	./test

.PHONY:clean
clean:
	rm test
