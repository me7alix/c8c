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
	return fail(get_loc(l), message);
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
		Location loc = get_loc(l);
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
				return fail(get_loc(l), "wrong keyword");
			}
		}

		AST *n = ast_new(AST_ID);
		n->loc = loc;
		n->as.id = id;
		return spc_ok(n, free);
	}

	return fail(get_loc(l), "identifier expected");
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

		while (
			(peek(l) >= 'A' && peek(l) <= 'F') ||
			(peek(l) >= 'a' && peek(l) <= 'f') ||
			peek(l) == 'x' || peek(l) == 'b' || isdigit(peek(l))
		) {
			if ((peek(l) == 'x' || peek(l) == 'b') && (count != 1 || digits[0] != '0')) {
				fatal(get_loc(l), "incorrect number");
			}
			digits[count++] = next(l);
		}

		unsigned long long num;
		if (digits[1] == 'x') {
			num = strtoull(digits + 2, NULL, 16);
		} else if (digits[1] == 'b') {
			num = strtoull(digits + 2, NULL, 2);
		} else {
			num = strtoull(digits, NULL, 10);
		}

		AST *n = ast_new(AST_OPR);
		n->loc = get_loc(l);
		n->as.opr.kind = OPR_IMM;
		n->as.opr.val = num;

		return spc_ok(n, free);
	}

	return fail(get_loc(l), "not a number");
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
		n->loc = get_loc(l);
		n->as.opr.kind = OPR_VREG;

		if (isdigit(id)) {
			n->as.opr.val = id - '0';
			return spc_ok(n, free);
		} else if (id >= 'a' && id <= 'f') {
			n->as.opr.val = id - 'a' + 10;
			return spc_ok(n, free);
		}
	} else if (peek(l) == 'i') {
		AST *n = ast_new(AST_OPR);
		n->loc = get_loc(l);
		n->as.opr.kind = OPR_IREG;
		next(l);
		return spc_ok(n, free);
	} else if (peek(l) == 's' || peek(l) == 'd') {
		Location loc = get_loc(l);
		char fl = next(l);

		if (peek(l) == 't') {
			AST *n = ast_new(AST_OPR);
			n->as.opr.kind = fl == 's' ? OPR_ST : OPR_DT;
			n->loc = loc;
			next(l);
			return spc_ok(n, free);
		}
	}

	return fail(get_loc(l), "not a register");
}

SPC_Parser *preg() {
	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = pregf;
	return p;
}

SPC_Result fold_math(int _, void **args) {
	AST *n = ast_new(AST_MATH);
	n->loc = ((AST*)args[0])->loc;
	n->as.math.lhs = args[0];
	n->as.math.rhs = args[2];

	switch ((char)args[1]) {
		case '+': n->as.math.op = MOP_ADD; break;
		case '-': n->as.math.op = MOP_SUB; break;
		case '=': n->as.math.op = MOP_ASG; break;
	}

	return spc_ok(n, free);
}

SPC_Result fold_items(int n, void **args) {
	if (n == 0) {
		return spc_ok(NULL, NULL);
	}

	AST *cur = args[0];
	for (int i = 1; i < n; i++) {
		cur->next = args[i];
		cur = cur->next;
	}

	return spc_ok(args[0], free);
}

SPC_Result fold_proc(int _, void **args) {
	AST *n = ast_new(AST_PROC);
	n->loc = ((AST*)args[0])->loc;
	n->as.proc.id = ((AST*)args[1])->as.id;
	n->as.proc.block = args[2];
	return spc_ok(n, free);
}

SPC_Result fold_loop(int _, void **args) {
	AST *n = ast_new(AST_LOOP);
	n->loc = ((AST*)args[0])->loc;
	n->as.loop.block = args[1];
	return spc_ok(n, free);
}

SPC_Result map_prog(void *data) {
	AST *n = ast_new(AST_PROG);
	n->as.prog = data;
	return spc_ok(n, free);
}

SPC_Result map_block(void *data) {
	AST *n = ast_new(AST_BLOCK);
	n->as.block = data;
	return spc_ok(n, free);
}

SPC_Result map_ret(void *data) {
	AST *n = ast_new(AST_RETURN);
	n->loc = ((AST*)data)->loc;
	return spc_ok(n, free);
}

SPC_Result fold_call(int _, void **args) {
	AST *n = ast_new(AST_CALL);
	n->loc = ((AST*)args[0])->loc;
	n->as.call.id = ((AST*)args[1])->as.id;
	return spc_ok(n, free);
}

SPC_Result fold_while(int _, void **args) {
	AST *n = ast_new(AST_WHILE);
	n->loc = ((AST*)args[0])->loc;
	n->as.whiles.cond = args[1];
	n->as.whiles.block = args[2];
	return spc_ok(n, free);
}

SPC_Result map_cls(void *data) {
	AST *n = ast_new(AST_CLS);
	n->loc = ((AST*)data)->loc;
	return spc_ok(n, free);
}

SPC_Result fold_ifs(int _, void **args) {
	AST *n = ast_new(AST_IF);
	n->loc = ((AST*)args[0])->loc;
	n->as.ifs.cond = args[1];
	n->as.ifs.block = args[2];
	return spc_ok(n, free);
}

SPC_Result fold_if(int _, void **args) {
	AST *n = ast_new(AST_IF);
	n->loc = ((AST*)args[0])->loc;
	n->as.ifs.cond = args[1];
	n->as.ifs.block = args[2];
	return spc_ok(n, free);
}

SPC_Result fold_else(int _, void **args) {
	AST *n = ast_new(AST_IF);
	n->loc = ((AST*)args[0])->loc;
	n->as.ifs.cond = NULL;
	n->as.ifs.block = args[1];
	return spc_ok(n, free);
}

SPC_Result fold_alias(int _, void **args) {
	AST *n = ast_new(AST_ALIAS);
	n->loc = ((AST*)args[0])->loc;
	n->as.alias.id = ((AST*)args[1])->as.id;
	n->as.alias.opr = args[2];
	return spc_ok(n, free);
}

SPC_Result fold_elif(int n, void **args) {
	if (n == 0) {
		return spc_ok(NULL, NULL);
	}

	AST *cur = args[0];
	for (int i = 1; i < n; i++) {
		cur->as.ifs.next = args[i];
		cur = cur->as.ifs.next;
	}

	return spc_ok(args[0], free);
}

SPC_Result fold_ifc(int _, void **args) {
	AST *n = args[0];

	if (args[1]) {
		n->as.ifs.next = args[1];
		n->as.ifs.next->as.ifs.next = args[2];
	} else {
		n->as.ifs.next = args[2];
	}

	return spc_ok(n, free);
}

SPC_Result fold_cop(int n, void **args) {
	if (n > 1) {
		switch ((char)args[0]) {
			case '=': return spc_ok((void*)COP_EQ, NULL);
			case '!': return spc_ok((void*)COP_NOT_EQ, NULL);
			case '>': return spc_ok((void*)COP_GREATER_EQ, NULL);
			case '<': return spc_ok((void*)COP_LESS_EQ, NULL);
		}
	} else {
		switch ((char)args[0]) {
			case '>': return spc_ok((void*)COP_GREATER, NULL);
			case '<': return spc_ok((void*)COP_LESS, NULL);
		}
	}

	return spc_fatal("wrong condition operator", NULL);
}

SPC_Result fold_cond(int _, void **args) {
	AST *n = ast_new(AST_COND);
	n->loc = ((AST*)args[0])->loc;
	n->as.cond.lhs = args[0];
	n->as.cond.op = (int)args[1];
	n->as.cond.rhs = args[2];
	return spc_ok(n, free);
}

SPC_Result fold_draw(int _, void **args) {
	AST *n = ast_new(AST_DRAW);
	n->loc = ((AST*)args[0])->loc;
	n->as.draw.data_id = ((AST*)args[1])->as.id;
	n->as.draw.x = args[2];
	n->as.draw.y = args[3];
	return spc_ok(n, free);
}

SPC_Result fold_bytes(int _, void **args) {
	AST *n = ast_new(AST_BYTES);
	n->as.bytes.id = ((AST*)args[0])->as.id;

	uint8_t data[1 << 16];
	size_t count = 0;

	for (AST *c = args[1]; c; c = c->next) {
		data[count++] = c->as.opr.val;
	}

	n->as.bytes.data = malloc(count);
	n->as.bytes.count = count;
	memcpy(n->as.bytes.data, data, count);

	return spc_ok(n, free);
}

SPC_Parser *parser() {
	SPC_Parser **block = malloc(sizeof(void*));
	SPC_Parser *lazy_block = spc_lazy(block);

	SPC_Parser *add   = spc_keep_left(pch('+'), pch('='));
	SPC_Parser *sub   = spc_keep_left(pch('-'), pch('='));
	SPC_Parser *mop   = spc_choice(3, add, sub, pch('='));

	SPC_Parser *gte   = spc_sequence(fold_cop, 2, pch('>'), pch('='));
	SPC_Parser *lte   = spc_sequence(fold_cop, 2, pch('<'), pch('='));
	SPC_Parser *ne    = spc_sequence(fold_cop, 2, pch('!'), pch('='));
	SPC_Parser *eq    = spc_sequence(fold_cop, 2, pch('='), pch('='));
	SPC_Parser *lt    = spc_sequence(fold_cop, 1, pch('<'));
	SPC_Parser *gt    = spc_sequence(fold_cop, 1, pch('>'));
	SPC_Parser *cop   = spc_choice(6, lt, gt, gte, lte, ne, eq);

	SPC_Parser *opr   = spc_choice(3, pnum(), preg(), pid(NULL));
	SPC_Parser *cond  = spc_sequence(fold_cond, 3, opr, cop, opr);
	SPC_Parser *math  = spc_sequence(fold_math, 3, opr, mop, opr);

	SPC_Parser *cls   = spc_map(map_cls, pid("cls"));
	SPC_Parser *call  = spc_sequence(fold_call, 2, pid("call"), pid(NULL));
	SPC_Parser *draw  = spc_sequence(fold_draw, 4, pid("draw"), pid(NULL), opr, opr);
	SPC_Parser *loop  = spc_sequence(fold_loop, 2, pid("loop"), lazy_block);
	SPC_Parser *ret   = spc_map(map_ret, pid("return"));
	SPC_Parser *whs   = spc_sequence(fold_while, 3, pid("while"), cond, lazy_block);
	SPC_Parser *elses = spc_sequence(fold_else, 2, pid("else"), lazy_block);
	SPC_Parser *elifs = spc_sequence(fold_if, 3, pid("elif"), cond, lazy_block);
	SPC_Parser *ifs   = spc_sequence(fold_if, 3, pid("if"), cond, lazy_block);
	SPC_Parser *ifc   = spc_sequence(fold_ifc, 3, ifs, spc_optional(spc_many(fold_elif, elifs)), spc_optional(elses));
	SPC_Parser *stmt  = spc_choice(9, ifc, cls, draw, loop, whs, ret, ifs, call, math);

	           *block = spc_between(pch('{'), spc_map(map_block, spc_many(fold_items, stmt)), pch('}'));
	SPC_Parser *alias = spc_sequence(fold_alias, 3, pid("alias"), pch('='), opr);
	SPC_Parser *proc  = spc_sequence(fold_proc, 3, pid("proc"), pid(NULL), *block);
	SPC_Parser *array = spc_between(pch('['), spc_sep_by(fold_items, pnum(), pch(',')), pch(']'));
	SPC_Parser *bytes = spc_sequence(fold_bytes, 2, spc_between(pid("bytes"), pid(NULL), pch('=')), array);
	/* SPC_Parser *words = spc_sequence(2, ..., spc_between(pid("words"), pid(NULL), pch('=')), array); */
	SPC_Parser *hld   = spc_choice(3, alias, proc, bytes);
	return              spc_map(map_prog, spc_many(fold_items, hld));

}
