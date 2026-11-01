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
	AST_CALL,
	AST_ID,
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
} AST_OprKind;

typedef struct {
	AST_OprKind kind;
	unsigned val;
} AST_Opr;

typedef struct AST AST;
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
			size_t count;
		} bytes;
		struct {
			char *id;
		} call;
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

void fintend(FILE *f, int intend, int depth) {
	for (int i = 0; i < intend * depth; i++) {
		fprintf(f, " ");
	}
}

void ast_dump(FILE *f, AST *n, int intend, int depth) {
	switch (n->kind) {
	case AST_PROG:
		for (AST *c = n->as.prog; c; c = c->next)
			ast_dump(f, c, intend, depth);
		break;

	case AST_PROC:
		fprintf(f, "proc %s {\n", n->as.proc.id);
		ast_dump(f, n->as.proc.block, intend, depth + 1);
		fprintf(f, "}\n");
		break;

	case AST_MATH:
		fintend(f, intend, depth);
		ast_dump(f, n->as.math.lhs, intend, depth);
		fprintf(f, " ");
		switch (n->as.math.op) {
			case MOP_ADD: fprintf(f, "+= "); break;
			case MOP_SUB: fprintf(f, "-= "); break;
			case MOP_AND: fprintf(f, "&= "); break;
			case MOP_OR:  fprintf(f, "|= "); break;
			case MOP_XOR: fprintf(f, "^= "); break;
			case MOP_ASG: fprintf(f, "= "); break;
		}
		ast_dump(f, n->as.math.rhs, intend, depth);
		fprintf(f, "\n");
		break;

	case AST_COND:
		ast_dump(f, n->as.cond.lhs, intend, depth);
		fprintf(f, " ");
		switch (n->as.cond.op) {
			case COP_EQ:         fprintf(f, "== "); break;
			case COP_NOT_EQ:     fprintf(f, "!= "); break;
			case COP_GREATER:    fprintf(f, "> ");  break;
			case COP_LESS:       fprintf(f, "< ");  break;
			case COP_GREATER_EQ: fprintf(f, ">= "); break;
			case COP_LESS_EQ:    fprintf(f, "<= "); break;
		}
		ast_dump(f, n->as.cond.rhs, intend, depth);
		break;

	case AST_BLOCK:
		for (AST *c = n->as.block; c; c = c->next)
			ast_dump(f, c, intend, depth);
		break;

	case AST_IF:
		fintend(f, intend, depth);
		fprintf(f, "if ");
		ast_dump(f, n->as.ifs.cond, intend, depth + 1);
		fprintf(f, " {\n");
		ast_dump(f, n->as.ifs.block, intend, depth + 1);
		fintend(f, intend, depth);
		fprintf(f, "}\n");
		break;

	case AST_WHILE:
		fintend(f, intend, depth);
		fprintf(f, "while ");
		ast_dump(f, n->as.whiles.cond, intend, depth + 1);
		fprintf(f, " {\n");
		ast_dump(f, n->as.whiles.block, intend, depth + 1);
		fintend(f, intend, depth);
		fprintf(f, "}\n");
		break;

	case AST_BYTES:
		fintend(f, intend, depth);
		fprintf(f, "bytes %s = [", n->as.bytes.id);
		for (size_t i = 0; i < n->as.bytes.count; i++) {
			fprintf(f, "%u", n->as.bytes.data[i]);
			if (i < n->as.bytes.count - 1) fprintf(f, ", ");
		}
		fprintf(f, "]\n");
		break;

	case AST_RETURN:
		fintend(f, intend, depth);
		fprintf(f, "return\n");
		break;

	case AST_CALL:
		fintend(f, intend, depth);
		fprintf(f, "call %s\n", n->as.call.id);
		break;

	case AST_OPR:;
		unsigned val = n->as.opr.val;
		switch (n->as.opr.kind) {
			case OPR_IMM:  fprintf(f, "%u", val);  break;
			case OPR_VREG: fprintf(f, "v%u", val); break;
			case OPR_IREG: fprintf(f, "i");        break;
		}
	}
}
