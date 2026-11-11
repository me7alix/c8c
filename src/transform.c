#include "parser.c"

typedef struct {
	char *id;
	AST *opr;
} Alias;

typedef struct {
	Alias *items;
	size_t count;
	size_t capacity;
} Aliases;

typedef struct {
	struct {
		Aliases *items;
		size_t count;
		size_t capacity;
	} stack;
} TransformCtx;

#define da_append(da, item)                                    \
	do {                                                       \
		if ((da)->capacity == 0) (da)->capacity = 256;         \
		if ((da)->count >= (da)->capacity - 1 || !(da)->items) \
			(da)->items = realloc((da)->items, sizeof(*(da)->items) * ((da)->capacity *= 2)); \
		(da)->items[(da)->count++] = (item);                   \
	} while (0)

void push_stack(TransformCtx *ctx) {
	da_append(&ctx->stack, ((Aliases){0}));
}

void pop_stack(TransformCtx *ctx) {
	ctx->stack.count--;
}

Alias *get_alias(TransformCtx *ctx, char *id) {
	for (size_t i = 0; i < ctx->stack.count; i++) {
		for (size_t j = 0; j < ctx->stack.items[i].count; j++) {
			if (strcmp(ctx->stack.items[i].items[j].id, id) == 0) {
				return &ctx->stack.items[i].items[j];
			}
		}
	}
	return NULL;
}

void append_alias(TransformCtx *ctx, AST *n) {
	size_t last = ctx->stack.count - 1;
	Alias alias = {n->as.alias.id, n->as.alias.opr};
	da_append(&ctx->stack.items[last], alias);
}

void transform_ast(TransformCtx *ctx, AST *n) {
	switch (n->kind) {
	case AST_PROC:
		transform_ast(ctx, n->as.proc.block);
		break;

	case AST_PROG:
		push_stack(ctx);
		for (AST *c = n->as.prog; c; c = c->next)
			transform_ast(ctx, c);
		pop_stack(ctx);
		break;

	case AST_MATH:
		transform_ast(ctx, n->as.math.lhs);
		transform_ast(ctx, n->as.math.rhs);
		break;

	case AST_COND:
		transform_ast(ctx, n->as.cond.lhs);
		transform_ast(ctx, n->as.cond.rhs);
		break;

	case AST_BLOCK: {
		push_stack(ctx);
		for (AST *c = n->as.block; c; c = c->next)
			transform_ast(ctx, c);
		pop_stack(ctx);
	} break;

	case AST_IF:
		transform_ast(ctx, n->as.ifs.cond);
		transform_ast(ctx, n->as.ifs.block);
		break;

	case AST_WHILE:
		transform_ast(ctx, n->as.whiles.cond);
		transform_ast(ctx, n->as.whiles.block);
		break;

	case AST_LOOP:
		transform_ast(ctx, n->as.loop.block);
		break;

	case AST_CALL:
		transform_ast(ctx, n->as.call.id);
		break;

	case AST_ID: {
		Alias *al = get_alias(ctx, n->as.id);
		if (al) *n = *(al->opr);
	} break;

	case AST_ALIAS: {
		append_alias(ctx, n);
	} break;

	case AST_DRAW:
		transform_ast(ctx, n->as.draw.data_id);
		transform_ast(ctx, n->as.draw.x);
		transform_ast(ctx, n->as.draw.y);
		break;

	default:;
	}
}
