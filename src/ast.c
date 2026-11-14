#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef enum {
	AST_PROG,
	AST_PROC,
	AST_BLOCK,
	AST_MATH,
	AST_COND,
	AST_OPR,
	AST_IF,
	AST_WHILE,
	AST_BYTES,
	AST_WORDS,
	AST_RETURN,
	AST_DRAW,
	AST_DRAWI,
	AST_LOOP,
	AST_CALL,
	AST_CLS,
	AST_ID,
	AST_ALIAS,
	AST_ARROF,
	AST_MACRO,
	AST_MCALL,
} AST_Kind;

typedef enum {
	MOP_ADD,
	MOP_SUB,
	MOP_AND,
	MOP_OR,
	MOP_XOR,
	MOP_ASG,
} AST_MathOp;

typedef enum {
	COP_EQ,
	COP_NOT_EQ,
	COP_GREATER,
	COP_LESS,
	COP_GREATER_EQ,
	COP_LESS_EQ,
} AST_CondOp;

typedef enum {
	OPR_IMM,
	OPR_VREG,
	OPR_IREG,
	OPR_DT,
	OPR_ST,
	OPR_MEM,
	OPR_ARR,
} AST_OprKind;

typedef struct AST AST;

typedef struct {
	AST_OprKind kind;
	unsigned val;
	AST *ptr, *idx;
} AST_Opr;

struct AST {
	AST_Kind kind;
	Location loc;
	AST *next;

	union {
		struct {
			AST_MathOp op;
			AST *lhs;
			AST *rhs;
		} math;
		struct {
			AST_CondOp op;
			AST *lhs;
			AST *rhs;
		} cond;
		struct {
			char *id;
			AST *block;
		} proc;
		struct {
			AST *cond;
			AST *block;
		} whiles;
		struct {
			AST *cond;
			AST *block;
			AST *next;
		} ifs;
		struct {
			char *id;
			unsigned char *data;
			AST *count;
		} bytes;
		struct {
			AST *id;
		} call;
		struct {
			AST *block;
		} loop;
		struct {
			AST *data_id;
			AST *x, *y;
		} draw;
		struct {
			AST *x;
			AST *y;
			AST *n;
		} drawi;
		struct {
			char *id;
			AST *opr;
		} alias;
		struct {
			AST *size;
		} arrof;
		struct {
			char *id;
			AST *args;
			AST *block;
		} macro;
		struct {
			AST *id;
			AST *args;
		} mcall;
		AST_Opr opr;
		AST *prog;
		AST *block;
		char *id;
	} as;
};

AST *ast_new(int kind) {
	AST *n = calloc(1, sizeof(*n));
	n->kind = kind;
	return n;
}

char *my_strdup(char *src) {
	size_t len = strlen(src) + 1;
	char *n = malloc(len);
	memcpy(n, src, len);
	return n;
}

AST *ast_copy(AST *n) {
	if (!n) return NULL;

	AST *copy = ast_new(n->kind);
	copy->loc = n->loc;
	copy->next = ast_copy(n->next);

	switch (n->kind) {
	case AST_PROG:
		copy->as.prog = ast_copy(n->as.prog);
		break;

	case AST_PROC:
		copy->as.proc.id = n->as.proc.id ? my_strdup(n->as.proc.id) : NULL;
		copy->as.proc.block = ast_copy(n->as.proc.block);
		break;

	case AST_BLOCK:
		copy->as.block = ast_copy(n->as.block);
		break;

	case AST_MATH:
		copy->as.math.op = n->as.math.op;
		copy->as.math.lhs = ast_copy(n->as.math.lhs);
		copy->as.math.rhs = ast_copy(n->as.math.rhs);
		break;

	case AST_COND:
		copy->as.cond.op = n->as.cond.op;
		copy->as.cond.lhs = ast_copy(n->as.cond.lhs);
		copy->as.cond.rhs = ast_copy(n->as.cond.rhs);
		break;

	case AST_OPR:
		copy->as.opr.kind = n->as.opr.kind;
		copy->as.opr.val = n->as.opr.val;
		copy->as.opr.ptr = ast_copy(n->as.opr.ptr);
		copy->as.opr.idx = ast_copy(n->as.opr.idx);
		break;

	case AST_IF:
		copy->as.ifs.cond = ast_copy(n->as.ifs.cond);
		copy->as.ifs.block = ast_copy(n->as.ifs.block);
		copy->as.ifs.next = ast_copy(n->as.ifs.next); // Preserves elif/else chain
		break;

	case AST_WHILE:
		copy->as.whiles.cond = ast_copy(n->as.whiles.cond);
		copy->as.whiles.block = ast_copy(n->as.whiles.block);
		break;

	case AST_BYTES:
	case AST_WORDS:
		copy->as.bytes.id = n->as.bytes.id ? my_strdup(n->as.bytes.id) : NULL;
		copy->as.bytes.count = ast_copy(n->as.bytes.count);
		if (n->as.bytes.data && n->as.bytes.count) {
			assert(n->as.bytes.count->kind == AST_OPR);
			assert(n->as.bytes.count->as.opr.kind == OPR_IMM);
			size_t len = n->as.bytes.count->as.opr.val;
			copy->as.bytes.data = malloc(len);
			if (copy->as.bytes.data) {
				memcpy(copy->as.bytes.data, n->as.bytes.data, len);
			}
		} else {
			copy->as.bytes.data = NULL;
		}
		break;

	case AST_LOOP:
		copy->as.loop.block = ast_copy(n->as.loop.block);
		break;

	case AST_CALL:
		copy->as.call.id = ast_copy(n->as.call.id);
		break;

	case AST_DRAW:
		copy->as.draw.data_id = ast_copy(n->as.draw.data_id);
		copy->as.draw.x = ast_copy(n->as.draw.x);
		copy->as.draw.y = ast_copy(n->as.draw.y);
		break;

	case AST_DRAWI:
		copy->as.drawi.x = ast_copy(n->as.drawi.x);
		copy->as.drawi.y = ast_copy(n->as.drawi.y);
		copy->as.drawi.n = ast_copy(n->as.drawi.n);
		break;

	case AST_ID:
		copy->as.id = n->as.id ? my_strdup(n->as.id) : NULL;
		break;

	case AST_ALIAS:
		copy->as.alias.id = n->as.alias.id ? my_strdup(n->as.alias.id) : NULL;
		copy->as.alias.opr = ast_copy(n->as.alias.opr);
		break;

	case AST_ARROF:
		copy->as.arrof.size = ast_copy(n->as.arrof.size);
		break;

	case AST_MACRO:
		copy->as.macro.id = n->as.macro.id ? my_strdup(n->as.macro.id) : NULL;
		copy->as.macro.args = ast_copy(n->as.macro.args);
		copy->as.macro.block = ast_copy(n->as.macro.block);
		break;

	case AST_MCALL:
		copy->as.mcall.id = ast_copy(n->as.mcall.id);
		copy->as.mcall.args = ast_copy(n->as.mcall.args);
		break;

	case AST_RETURN:
	case AST_CLS:
		break;
	}

	return copy;
}

void fintend(FILE *f, int it, int dp) {
	for (int i = 0; i < it * dp; i++) {
		fprintf(f, " ");
	}
}

void ast_dump(FILE *f, AST *n, int it, int dp) {
	switch (n->kind) {
	case AST_PROG:
		for (AST *c = n->as.prog; c; c = c->next) {
			ast_dump(f, c, it, dp);
			fprintf(f, "\n");
		} break;

	case AST_PROC:
		fprintf(f, "proc %s {\n", n->as.proc.id);
		ast_dump(f, n->as.proc.block, it, dp + 1);
		fprintf(f, "}\n");
		break;

	case AST_MATH:
		fintend(f, it, dp);
		ast_dump(f, n->as.math.lhs, it, dp);
		fprintf(f, " ");
		switch (n->as.math.op) {
			case MOP_ADD: fprintf(f, "+= "); break;
			case MOP_SUB: fprintf(f, "-= "); break;
			case MOP_AND: fprintf(f, "&= "); break;
			case MOP_OR:  fprintf(f, "|= "); break;
			case MOP_XOR: fprintf(f, "^= "); break;
			case MOP_ASG: fprintf(f, "= "); break;
		}
		ast_dump(f, n->as.math.rhs, it, dp);
		fprintf(f, "\n");
		break;

	case AST_COND:
		ast_dump(f, n->as.cond.lhs, it, dp);
		fprintf(f, " ");
		switch (n->as.cond.op) {
			case COP_EQ:         fprintf(f, "== "); break;
			case COP_NOT_EQ:     fprintf(f, "!= "); break;
			case COP_GREATER:    fprintf(f, "> ");  break;
			case COP_LESS:       fprintf(f, "< ");  break;
			case COP_GREATER_EQ: fprintf(f, ">= "); break;
			case COP_LESS_EQ:    fprintf(f, "<= "); break;
		}
		ast_dump(f, n->as.cond.rhs, it, dp);
		break;

	case AST_BLOCK:
		for (AST *c = n->as.block; c; c = c->next)
			ast_dump(f, c, it, dp);
		break;

	case AST_IF: {
		fintend(f, it, dp);

		for (AST *c = n; c; c = c->as.ifs.next) {
			if (c == n) {
				fprintf(f, "if ");
			} else if (!c->as.ifs.cond) {
				fprintf(f, "else ");
			} else {
				fprintf(f, "elif ");
			}

			if (c->as.ifs.cond) {
				ast_dump(f, c->as.ifs.cond, it, dp + 1);
				fprintf(f, " {\n");
			} else {
				fprintf(f, "{\n");
			}

			ast_dump(f, c->as.ifs.block, it, dp + 1);
			fintend(f, it, dp);

			if (c->as.ifs.next) {
				fprintf(f, "} ");
			} else {
				fprintf(f, "}\n");
			}
		}
	} break;

	case AST_MCALL:
		fintend(f, it, dp);
		fprintf(f, "%s", n->as.mcall.id->as.id);
		fprintf(f, "(");
		for (AST *c = n->as.mcall.args; c; c = c->next) {
			ast_dump(f, c, it, dp+1);
			if (c->next) fprintf(f, ", ");
		}
		fprintf(f, ")\n");
		break;

	case AST_MACRO:
		fintend(f, it, dp);
		fprintf(f, "macro %s", n->as.macro.id);
		fprintf(f, "(");
		for (AST *c = n->as.macro.args; c; c = c->next) {
			ast_dump(f, c, it, dp+1);
			if (c->next) fprintf(f, ", ");
		}
		fprintf(f, ") {\n");
		ast_dump(f, n->as.macro.block, it, dp+1);
		fprintf(f, "}\n");
		break;

	case AST_WHILE:
		fintend(f, it, dp);
		fprintf(f, "while ");
		ast_dump(f, n->as.whiles.cond, it, dp + 1);
		fprintf(f, " {\n");
		ast_dump(f, n->as.whiles.block, it, dp + 1);
		fintend(f, it, dp);
		fprintf(f, "}\n");
		break;

	case AST_LOOP:
		fintend(f, it, dp);
		if (n->as.loop.block->as.block) {
			fprintf(f, "loop {\n");
			ast_dump(f, n->as.loop.block, it, dp + 1);
			fintend(f, it, dp);
			fprintf(f, "}\n");
		} else {
			fprintf(f, "loop {}\n");
		} break;

	case AST_BYTES:
		fintend(f, it, dp);
		if (!n->as.bytes.data) {
			fprintf(f, "bytes %s = ", n->as.bytes.id);
			fprintf(f, "arrayOf(");
			ast_dump(f, n->as.bytes.count, it, dp+1);
			fprintf(f, ")\n");
		} else {
			assert(n->as.bytes.count->kind == AST_OPR);
			assert(n->as.bytes.count->as.opr.kind == OPR_IMM);
			fprintf(f, "bytes %s = [", n->as.bytes.id);
			for (size_t i = 0; i < n->as.bytes.count->as.opr.val; i++) {
				fprintf(f, "%u", n->as.bytes.data[i]);
				if (i < n->as.bytes.count->as.opr.val - 1) fprintf(f, ", ");
			}
			fprintf(f, "]\n");
		}
		break;

	case AST_RETURN:
		fintend(f, it, dp);
		fprintf(f, "return\n");
		break;

	case AST_CALL:
		fintend(f, it, dp);
		fprintf(f, "call %s\n", n->as.call.id->as.id);
		break;

	case AST_ID:
		fprintf(f, "%s", n->as.id);
		break;

	case AST_DRAW:
		fintend(f, it, dp);
		fprintf(f, "draw %s, ", n->as.draw.data_id->as.id);
		ast_dump(f, n->as.draw.x, it, dp+1);
		fprintf(f, ", ");
		ast_dump(f, n->as.draw.y, it, dp+1);
		fprintf(f, "\n");
		break;

	case AST_DRAWI:
		fintend(f, it, dp);
		fprintf(f, "drawi ");
		ast_dump(f, n->as.drawi.x, it, dp+1);
		fprintf(f, ", ");
		ast_dump(f, n->as.drawi.y, it, dp+1);
		fprintf(f, ", ");
		ast_dump(f, n->as.drawi.n, it, dp+1);
		fprintf(f, "\n");
		break;

	case AST_CLS:
		fintend(f, it, dp);
		fprintf(f, "cls\n");
		break;

	case AST_ALIAS:
		fintend(f, it, dp);
		fprintf(f, "alias %s = ", n->as.alias.id);
		ast_dump(f, n->as.alias.opr, it, dp);
		fprintf(f, "\n");
		break;

	case AST_OPR:;
		unsigned val = n->as.opr.val;
		switch (n->as.opr.kind) {
			case OPR_IMM:  fprintf(f, "%u", val);  break;
			case OPR_VREG: fprintf(f, "v%u", val); break;
			case OPR_IREG: fprintf(f, "i");        break;
			case OPR_DT:   fprintf(f, "dt");       break;
			case OPR_ST:   fprintf(f, "st");       break;
			case OPR_MEM:  fprintf(f, "[i]");      break;
			case OPR_ARR: {
				ast_dump(f, n->as.opr.ptr, it, dp+1);
				fprintf(f, "[");
				ast_dump(f, n->as.opr.idx, it, dp+1);
				fprintf(f, "]");
			} break;
		}
	}
}
