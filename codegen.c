#include "parser.c"

typedef struct {
	unsigned ptr;
} CodegenCtx;

void emit_prog(FILE *f, AST *n) {
	CodegenCtx ctx = {0x200, f};
	for (AST *c = n->as.prog; c; c = c->next) {
		switch (c->kind) {
			case AST_PROC:  emit_proc(f, &ctx, n);  break;
			case AST_BYTES: emit_bytes(f, &ctx, n); break;
		}
	}
}
