WARNS := \
-Wunused-variable \
-Wno-switch \
-Wno-void-pointer-to-int-cast \
-Wno-int-to-void-pointer-cast

c8c: main.c scanner.c ast.c parser.c codegen.c
	clang $(WARNS) -ggdb main.c -o c8c

test: c8c
	./c8c test1.c8 result

.PHONY: test
