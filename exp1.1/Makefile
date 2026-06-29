CC = gcc
FLEX = flex
CFLAGS = -Wall -Wextra -std=c99

.PHONY: all clean test

all: lexer

lexer: lex.yy.c
	$(CC) $(CFLAGS) lex.yy.c -o lexer

lex.yy.c: lexer.l
	$(FLEX) lexer.l

test: lexer
	./lexer tests/lex_ok.cmm
	./lexer tests/lex_error.cmm || true

clean:
	rm -f lexer lex.yy.c
