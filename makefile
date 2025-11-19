CC = cc
CFLAGS = -std=c11 -Wall -Wextra -O2

OBJS = lexer.o parser.o codegen.o compiler.o

all: compiler

compiler: $(OBJS)
	$(CC) $(CFLAGS) -o compiler $(OBJS)

lexer.o: lexer.c compiler.h
	$(CC) $(CFLAGS) -c lexer.c

parser.o: parser.c compiler.h
	$(CC) $(CFLAGS) -c parser.c

codegen.o: codegen.c compiler.h
	$(CC) $(CFLAGS) -c codegen.c

compiler.o: compiler.c compiler.h
	$(CC) $(CFLAGS) -c compiler.c

clean:
	rm -f *.o compiler output.c app

.PHONY: all clean
