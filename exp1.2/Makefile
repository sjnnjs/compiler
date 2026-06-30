CC = gcc
FLEX = flex
BISON = bison
CFLAGS = -Wall -Wextra -std=c99

.PHONY: all clean test

all: cc

cc: parser.tab.c lex.yy.c ast.c ast.h
	$(CC) $(CFLAGS) parser.tab.c lex.yy.c ast.c -o cc

parser.tab.c parser.tab.h: parser.y ast.h
	$(BISON) -d parser.y

lex.yy.c: lexer.l parser.tab.h ast.h
	$(FLEX) lexer.l

test: cc
	./cc tests/syntax_ok.cmm
	./cc tests/syntax_error.cmm || true
	./cc tests/lex_error.cmm || true

sample-test: cc
	@for file in tests/required_*.cmm tests/optional_*.cmm; do \
		echo "==== $$file ===="; \
		./cc $$file || true; \
	done

clean:
	rm -f cc lex.yy.c parser.tab.c parser.tab.h
