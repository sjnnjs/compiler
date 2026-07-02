CC = gcc
FLEX = flex
BISON = bison
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L

.PHONY: all clean test

all: cc

cc: parser.tab.c lex.yy.c ast.c semantic.c ir.c ast.h semantic.h ir.h
	$(CC) $(CFLAGS) parser.tab.c lex.yy.c ast.c semantic.c ir.c -o cc

parser.tab.c parser.tab.h: parser.y ast.h semantic.h ir.h
	$(BISON) -d parser.y

lex.yy.c: lexer.l parser.tab.h ast.h
	$(FLEX) lexer.l

test: cc
	@mkdir -p test/out
	@for file in test/*.cmm; do \
		echo "==== $$file ===="; \
		out="test/out/$$(basename "$$file" .cmm).ir"; \
		./cc "$$file" "$$out" && sed -n '1,120p' "$$out" || true; \
	done

clean:
	rm -f cc lex.yy.c parser.tab.c parser.tab.h
	rm -rf test/out
