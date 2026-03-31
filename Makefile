export
CC=g++
LD=g++
INCLUDE=-I $(shell pwd)/include -I $(shell pwd)/src/include
CF=-O1 --std=c++17
CFLAG=${CF} ${INCLUDE}

.PHONY: all clean
all:
	${MAKE} -C lib all
	${MAKE} -C src all
	@echo -e '\n'Build Finished OK

clean:
	${MAKE} -C lib clean
	${MAKE} -C src clean
	$(shell rm -rf server)
	@echo -e '\n'Clean Finished
