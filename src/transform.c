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
	Aliases *items;
	size_t count;
	size_t capacity;
} AliasesStack;

typedef struct {
	AliasesStack stack;
} TransformCtx;

/*
void transform_ast(TransformCtx *ctx, AST *n) {
	switch (n->kind) {
	case AST_PROC:
		transform_ast(ctx, n->as.proc.block);
		break;

	case AST_PROG:
		for (AST *c = n->as.prog; c; c = c->next) {
			transform_ast(ctx, c);
		} break;

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

	case AST_BLOCK: {


		for (AST *c = n->as.block; c; c = c->next) {
			ast_dump(f, c, it, dp);
		}
	} break;

	case AST_IF:
		fintend(f, it, dp);
		fprintf(f, "if ");
		ast_dump(f, n->as.ifs.cond, it, dp + 1);
		fprintf(f, " {\n");
		ast_dump(f, n->as.ifs.block, it, dp + 1);
		fintend(f, it, dp);
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
		fprintf(f, "bytes %s = [", n->as.bytes.id);
		for (size_t i = 0; i < n->as.bytes.count; i++) {
			fprintf(f, "%u", n->as.bytes.data[i]);
			if (i < n->as.bytes.count - 1) fprintf(f, ", ");
		}
		fprintf(f, "]\n");
		break;

	case AST_RETURN:
		fintend(f, it, dp);
		fprintf(f, "return\n");
		break;

	case AST_CALL:
		fintend(f, it, dp);
		fprintf(f, "call %s\n", n->as.call.id);
		break;

	case AST_ID:
		fprintf(f, "%s", n->as.id);
		break;

	case AST_DRAW:
		fintend(f, it, dp);
		fprintf(f, "draw %s ", n->as.draw.data_id);
		ast_dump(f, n->as.draw.x, it, dp+1);
		fprintf(f, " ");
		ast_dump(f, n->as.draw.y, it, dp+1);
		fprintf(f, "\n");
		break;

	case AST_CLS:
		fintend(f, it, dp);
		fprintf(f, "cls\n");
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
*/
