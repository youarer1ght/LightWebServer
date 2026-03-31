SRC=$(sort $(wildcard *.cpp))
OBJ=$(patsubst %.cpp,%.o,$(SRC))

all: $(OBJ)
	${LD} ../../lib/*.o $(OBJ) -o ../../server

%.o: %.cpp
	${CC}  ${CFLAG} -c $<

clean:
	$(shell rm *.o 2>/dev/null)
