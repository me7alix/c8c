CC ?= clang
CFLAGS ?= -std=c99 -ggdb
CFLAGS += \
	-Wunused-variable \
	-Wreturn-type \
	-Wno-switch \
	-Wno-void-pointer-to-int-cast \
	-Wno-int-to-void-pointer-cast \
	-Wno-pointer-to-int-cast \
	-Wno-int-to-pointer-cast

c8c: src/main.c src/scanner.c src/ast.c src/parser.c src/codegen.c src/transform.c
	$(CC) $(CFLAGS) src/main.c -o c8c
