#include <ctype.h>
#include <string.h>
#include "scanner.c"
#include "ast.c"

#define SPC_IMPLEMENTATION
#include "spc.h"

typedef struct {
	Location loc;
	char message[256];
} Error;

SPC_Result fail(Location loc, char *msg) {
	static Error r; r.loc = loc;
	sprintf(r.message, "%s", msg);
	return spc_fail(&r, NULL);
}

SPC_Result fatal(Location loc, char *msg) {
	static Error r; r.loc = loc;
	sprintf(r.message, "%s", msg);
	return spc_fatal(&r, NULL);
}

typedef struct {
	char character;
} ParseCharCtx;

SPC_Result pchf(SPC_Parser *p, SPC_Input *inp) {
	ParseCharCtx *ctx = p->ctx;
	char ch = ctx->character;

	Scanner *l = inp->self;
	skip_ws(l);

	if (ch == peek(l)) {
		next(l);
		if (ch == '{' || ch == '[') {
			skip_ws(l);
			if (peek(l) == '\n') next(l);
		}
		return spc_ok((void*)ctx->character, NULL);
	}

	char message[256] = {0};
	sprintf(message, "character '%c' expected", ctx->character);
	return fail(loc(l), message);
}


SPC_Parser *pch(char ch) {
	SPC_Parser *p = malloc(sizeof(*p));
	ParseCharCtx *ctx = malloc(sizeof(*ctx));
	ctx->character = ch;
	p->parse = pchf;
	p->ctx = ctx;
	return p;
}

typedef struct {
	char *id;
} ParseIdCtx;

SPC_Result pidf(SPC_Parser *p, SPC_Input *inp) {
	ParseIdCtx *ctx = p->ctx;
	Scanner *l = inp->self;
	skip_ws(l);

	if (isalpha(peek(l)) || peek(l) == '_') {
		char *id = malloc(256);
		size_t count = 0;

		while (
			isalpha(peek(l)) ||
			isdigit(peek(l)) ||
			peek(l) == '_'
		) id[count++] = next(l);

		id[count] = '\0';

		if (ctx->id) {
			if (strcmp(ctx->id, id) != 0) {
				return fail(loc(l), "wrong keyword");
			}
		}

		return spc_ok(id, free);
	}

	return fail(loc(l), "identifier expected");
}

SPC_Parser *pid(char *id) {
	SPC_Parser *p = malloc(sizeof(*p));
	ParseIdCtx *ctx = malloc(sizeof(*ctx));
	ctx->id = id;
	p->parse = pidf;
	p->ctx = ctx;
	return p;
}

SPC_Result pnumf(SPC_Parser *p, SPC_Input *inp) {
	Scanner *l = inp->self;
	skip_ws(l);

	if (isdigit(peek(l))) {
		char digits[256] = {0};
		size_t count = 0;

		while (isdigit(peek(l)) || peek(l) == 'x' || peek(l) == 'b') {
			if ((peek(l) == 'x' || peek(l) == 'b') && (count != 1 || digits[0] != '0')) {
				fatal(loc(l), "incorrect number");
			}
			digits[count++] = next(l);
		}

		unsigned long long num = strtoull(digits, NULL, 0);

		AST *n = ast_new(AST_OPR);
		n->as.opr.kind = OPR_IMM;
		n->as.opr.val = num;

		return spc_ok(n, free);
	}

	return fail(loc(l), "not a number");
}

SPC_Parser *pnum() {
	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = pnumf;
	return p;
}

SPC_Result pregf(SPC_Parser *p, SPC_Input *inp) {
	Scanner *l = inp->self;
	skip_ws(l);

	if (peek(l) == 'v') {
		next(l);
		char id = next(l);

		AST *n = ast_new(AST_OPR);
		n->as.opr.kind = OPR_VREG;

		if (isdigit(id)) {
			n->as.opr.val = id - '0';
			return spc_ok(n, free);
		} else if (id >= 'a' && id <= 'f') {
			n->as.opr.val = id - 'a' + 10;
			return spc_ok(n, free);
		}
	} else if (peek(l) == 'i') {
		next(l);
		AST *n = ast_new(AST_OPR);
		n->as.opr.kind = OPR_IREG;
		return spc_ok(n, free);
	}

	return fail(loc(l), "not a register");
}

SPC_Parser *preg() {
	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = pregf;
	return p;
}

void *fold_math(int, void **args) {
	AST *n = ast_new(AST_MATH);
	n->as.math.lhs = args[0];
	n->as.math.rhs = args[2];

	switch ((char)args[1]) {
		case '+': n->as.math.op = MOP_ADD; break;
		case '-': n->as.math.op = MOP_SUB; break;
	}
}

void *fold_items(int n, void **args) {
	if (n == 0) {
		return NULL;
	}

	AST *cur = args[0];
	for (int i = 1; i < n; i++) {
		cur->next = args[i];
		cur = cur->next;
	}

	return args[0];
}

void *fold_proc(int, void **args) {
	AST *n = ast_new(AST_PROC);
	n->as.proc.id = args[0];
	n->as.proc.block = args[1];
	return n;
}

void *map_prog(void *data) {
	AST *n = ast_new(AST_PROG);
	n->as.prog = data;
	return n;
}

void *map_block(void *data) {
	AST *n = ast_new(AST_BLOCK);
	n->as.block = data;
	return n;
}

void *fold_while(int, void **args) {
	AST *n = ast_new(AST_WHILE);
	n->as.whiles.cond = args[0];
	n->as.whiles.block = args[1];
	return n;
}

void *fold_cond_op(int n, void **args) {
	if (n > 1) {
		switch ((char)args[0]) {
			case '=': return (void*)COP_EQ;
			case '!': return (void*)COP_NOT_EQ;
			case '>': return (void*)COP_GREATER_EQ;
			case '<': return (void*)COP_LESS_EQ;
		}
	} else {
		switch ((char)args[0]) {
			case '>': return (void*)COP_GREATER;
			case '<': return (void*)COP_LESS;
		}
	}
}

void *fold_cond(int, void **args) {
	AST *n = ast_new(AST_COND);
	n->as.cond.lhs = args[0];
	n->as.cond.op = (int)args[1];
	n->as.cond.rhs = args[2];
	return n;
}

void *fold_bytes(int, void **args) {
	AST *n = ast_new(AST_BYTES);
	n->as.bytes.id = args[0];

	unsigned char data[1 << 16];
	size_t count = 0;

	for (AST *c = args[1]; c; c = c->next) {
		data[count++] = c->as.opr.val;
	}

	n->as.bytes.data = malloc(count);
	n->as.bytes.count = count;
	memcpy(n->as.bytes.data, data, count);

	return n;
}

SPC_Parser *parser() {
	SPC_Fold f_math  = {fold_math, free};
	SPC_Fold f_proc  = {fold_proc, free};
	SPC_Fold f_items = {fold_items, free};
	SPC_Fold f_while = {fold_while, free};
	SPC_Fold f_cop   = {fold_cond_op, NULL};
	SPC_Fold f_cond  = {fold_cond, free};
	SPC_Fold f_bytes = {fold_bytes, free};
	SPC_Map  m_block = {map_block, free};
	SPC_Map  m_prog  = {map_prog, free};

	SPC_Parser **block = malloc(sizeof(void*));
	SPC_Parser *lazy_block = spc_lazy(block);

	SPC_Parser *add   = spc_keep_left(pch('+'), pch('='));
	SPC_Parser *sub   = spc_keep_left(pch('-'), pch('='));
	SPC_Parser *mop   = spc_choice(2, add, sub);

	SPC_Parser *gte   = spc_sequence(2, f_cop, pch('>'), pch('='));
	SPC_Parser *lte   = spc_sequence(2, f_cop, pch('<'), pch('='));
	SPC_Parser *ne    = spc_sequence(2, f_cop, pch('!'), pch('='));
	SPC_Parser *eq    = spc_sequence(2, f_cop, pch('='), pch('='));
	SPC_Parser *lt    = spc_sequence(1, f_cop, pch('<'));
	SPC_Parser *gt    = spc_sequence(1, f_cop, pch('>'));
	SPC_Parser *cop   = spc_choice(6, lt, gt, gte, lte, ne, eq);

	SPC_Parser *opr   = spc_choice(2, pnum(), preg());
	SPC_Parser *cond  = spc_sequence(3, f_cond, opr, cop, opr);
	SPC_Parser *math  = spc_sequence(3, f_math, opr, mop, opr);

	SPC_Parser *whs   = spc_sequence(2, f_while, spc_keep_right(pid("while"), cond), lazy_block);
	SPC_Parser *stmt  = spc_choice(2, math, whs);

	           *block = spc_between(pch('{'), spc_map(spc_many(stmt, f_items), m_block), pch('}'));
	SPC_Parser *proc  = spc_sequence(2, f_proc, spc_keep_right(pid("proc"), pid(NULL)), *block);
	SPC_Parser *array = spc_between(pch('['), spc_sep_by(pnum(), pch(','), f_items), pch(']'));
	SPC_Parser *bytes = spc_sequence(2, f_bytes, spc_between(pid("bytes"), pid(NULL), pch('=')), array);
	/* SPC_Parser *words = spc_sequence(2, ..., spc_between(pid("words"), pid(NULL), pch('=')), array); */
	SPC_Parser *hld   = spc_choice(2, proc, bytes);
	return              spc_map(spc_many(hld, f_items), m_prog);

}
