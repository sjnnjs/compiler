CC = gcc
FLEX = flex
BISON = bison
CFLAGS = -Wall -Wextra -std=c99

.PHONY: all clean test

all: cc

cc: parser.tab.c lex.yy.c ast.c semantic.c ast.h semantic.h
	$(CC) $(CFLAGS) parser.tab.c lex.yy.c ast.c semantic.c -o cc

parser.tab.c parser.tab.h: parser.y ast.h semantic.h
	$(BISON) -d parser.y

lex.yy.c: lexer.l parser.tab.h ast.h
	$(FLEX) lexer.l

test: cc
	@for file in test/*.cmm; do \
		echo "==== $$file ===="; \
		./cc $$file || true; \
	done

clean:
	rm -f cc lex.yy.c parser.tab.c parser.tab.h
