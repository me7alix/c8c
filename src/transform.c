#include "parser.c"

typedef struct {
	char *id;
	AST *opr;
} Alias;

typedef struct {
	char *id;
	AST *args;
	AST *block;
} Macro;

typedef struct {
	AST *dargs;
	AST *iargs;
} MCall;

typedef struct {
	MCall *items;
	size_t count;
	size_t capacity;
} MCalls;

typedef struct {
	Alias *items;
	size_t count;
	size_t capacity;
} Aliases;

typedef struct {
	Macro *items;
	size_t count;
	size_t capacity;
} Macros;

typedef struct {
	struct {
		Aliases *items;
		size_t count;
		size_t capacity;
	} aliases;
	struct {
		Macros *items;
		size_t count;
		size_t capacity;
	} macros;
	MCalls mcalls;
} TransformCtx;

#define da_append(da, item)                                    \
	do {                                                       \
		if ((da)->capacity == 0) (da)->capacity = 256;         \
		if ((da)->count >= (da)->capacity - 1 || !(da)->items) \
			(da)->items = realloc((da)->items, sizeof(*(da)->items) * ((da)->capacity *= 2)); \
		(da)->items[(da)->count++] = (item);                   \
	} while (0)

void push_stack(TransformCtx *ctx) {
	da_append(&ctx->aliases, ((Aliases){0}));
	da_append(&ctx->macros, ((Macros){0}));
}

void pop_stack(TransformCtx *ctx) {
	ctx->aliases.count--;
	ctx->macros.count--;
}

Alias *get_alias(TransformCtx *ctx, char *id) {
	for (int i = ctx->aliases.count - 1; i >= 0; i--) {
		for (int j = ctx->aliases.items[i].count - 1; j >= 0; j--) {
			if (strcmp(ctx->aliases.items[i].items[j].id, id) == 0) {
				return &ctx->aliases.items[i].items[j];
			}
		}
	}
	return NULL;
}

Macro *get_macro(TransformCtx *ctx, char *id) {
	for (int i = ctx->macros.count - 1; i >= 0; i--) {
		for (int j = ctx->macros.items[i].count - 1; j >= 0; j--) {
			if (strcmp(ctx->macros.items[i].items[j].id, id) == 0) {
				return &ctx->macros.items[i].items[j];
			}
		}
	}
	return NULL;
}

void append_alias(TransformCtx *ctx, char *id, AST *opr) {
	size_t last = ctx->aliases.count - 1;
	Alias alias = {id, opr};
	da_append(&ctx->aliases.items[last], alias);
}

void append_macro(TransformCtx *ctx, AST *n) {
	size_t last = ctx->macros.count - 1;
	Macro macro = {n->as.macro.id, n->as.macro.args, n->as.macro.block};
	da_append(&ctx->macros.items[last], macro);
}

void ast_insert(AST *prev, AST *n) {
	AST *next = prev->next;
	prev->next = n;
	n->next = next;
}

char *get_folder(char *file) {
	char *dir = my_strdup(file);
	size_t len = strlen(file);

	for (int i = len - 1; i >= 0; i--) {
		if (dir[i] == '/') {
			dir[i + 1] = '\0';
			break;
		}

		if (i == 0) {
			return NULL;
		}
	}

	return dir;
}

void transform_ast(TransformCtx *ctx, AST *n) {
	switch (n->kind) {
	case AST_PROC:
		transform_ast(ctx, n->as.proc.block);
		break;

	case AST_BYTES:
		transform_ast(ctx, n->as.bytes.count);
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
		if (n->as.ifs.cond)
			transform_ast(ctx, n->as.ifs.cond);
		transform_ast(ctx, n->as.ifs.block);
		if (n->as.ifs.next)
			transform_ast(ctx, n->as.ifs.next);
		break;

	case AST_WHILE:
		transform_ast(ctx, n->as.whiles.cond);
		transform_ast(ctx, n->as.whiles.block);
		break;

	case AST_OPR:
		if (n->as.opr.kind == OPR_ARR) {
			transform_ast(ctx, n->as.opr.as.arr.ptr);
			transform_ast(ctx, n->as.opr.as.arr.idx);
		} break;

	case AST_LOOP:
		transform_ast(ctx, n->as.loop.block);
		break;

	case AST_CALL:
		transform_ast(ctx, n->as.call.id);
		break;

	case AST_MACRO:
		append_macro(ctx, n);
		break;

	case AST_MCALL: {
		Macro *m = get_macro(ctx, n->as.mcall.id->as.id);
		if (!m) throw_error(n->loc, "no such macro");

		for (AST *ca = n->as.mcall.args; ca; ca = ca->next) {
			transform_ast(ctx, ca);
		}

		MCall mc = {0};
		mc.dargs = m->args;
		mc.iargs = n->as.mcall.args;
		da_append(&ctx->mcalls, mc);

		for (AST *da = mc.dargs, *ia = mc.iargs; da && ia; da = da->next, ia = ia->next) {
			if (da->next && !ia->next) {
				throw_error(n->loc, "not enough arguments");
			} else if (!da->next && ia->next) {
				throw_error(n->loc, "too many arguments");
			}
		}

		AST *next = n->next;
		*n = *ast_copy(m->block);
		n->next = next;
		transform_ast(ctx, n);

		ctx->mcalls.count--;
	} break;

	case AST_ID: {
		if (ctx->mcalls.count > 0) {
			MCall mc = ctx->mcalls.items[ctx->mcalls.count - 1];
			for (AST *da = mc.dargs, *ia = mc.iargs; da && ia; da = da->next, ia = ia->next) {
				if (strcmp(da->as.id, n->as.id) == 0) {
					AST *next = n->next;
					*n = *ia;
					n->next = next;
					return;
				}
			}
		}

		while (true) {
			if (n->kind != AST_ID) break;
			Alias *al = get_alias(ctx, n->as.id);
			if (al) {
				AST *next = n->next;
				*n = *(al->opr);
				n->next = next;
			} else {
				break;
			}
		}
	} break;

	case AST_WITH: {
		push_stack(ctx);
		for (AST *c = n->as.with.maps; c; c = c->next)
			transform_ast(ctx, c->as.map.arr);
		for (AST *c = n->as.with.maps; c; c = c->next)
			if (c->as.map.as->as.id)
				append_alias(ctx, c->as.map.as->as.id, c->as.map.reg);
		transform_ast(ctx, n->as.with.block);
		pop_stack(ctx);
	} break;

	case AST_ALIAS:
		append_alias(ctx, n->as.alias.id, n->as.alias.opr);
		break;

	case AST_DRAW:
		transform_ast(ctx, n->as.draw.data_id);
		transform_ast(ctx, n->as.draw.x);
		transform_ast(ctx, n->as.draw.y);
		break;

	case AST_BCD:
		transform_ast(ctx, n->as.bcd.reg);
		break;

	case AST_DRAWI:
		transform_ast(ctx, n->as.drawi.x);
		transform_ast(ctx, n->as.drawi.y);
		transform_ast(ctx, n->as.drawi.n);
		break;

	case AST_INCLUDE: {
		char *folder = get_folder(n->loc.file);
		if (!folder) throw_error(n->loc, "invalid file path");

		SPC_Parser *p = parser();

		char path[4096];
		sprintf(path, "%s%s", folder, n->as.include.path);

		Scanner *sc = sc_from_file(path);
		if (!sc) throw_error(n->loc, "file not found");

		SPC_Input inp = {sc, sc_mark, sc_unmark, sc_rewind};

		SPC_Result res = p->parse(p, &inp);
		if (res.status != SPC_OK) {
			Error *err = res.data;
			throw_error(err->loc, err->message);
		}

		AST *prog = res.data;
		AST *list = prog->as.prog;
		if (list) {
			AST *last;
			for (AST *c = list; c; c = c->next) {
				transform_ast(ctx, c);
				if (!c->next) last = c;
			}
			last->next = n->next;
			*n = *list;
	 	}
	} break;

	default:;
	}
}
