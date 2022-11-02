CFLAGS += -Wall -g3 -O3

compiler: compiler.o
	${CC} $^ -o $@

compiler.o: compiler.c

clean:
	rm -f compiler compiler.o
