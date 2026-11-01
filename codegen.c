#include <stdint.h>
#include "parser.c"

typedef struct {
	char *id;
	uint16_t addr;
} Proc;

typedef struct {
	Proc *items;
	size_t count;
	size_t capacity;
} ProcCont;

typedef struct {
	char *id;
	uint16_t addr;
	size_t len;
} Data;

typedef struct {
	Data *items;
	size_t count;
	size_t capacity;
} DataCont;

typedef struct {
	ProcCont proc;
	DataCont data;
	uint16_t addr;
	Proc *cur_proc;
} CodegenCtx;

#define da_append(da, item)                                    \
	do {                                                       \
		if ((da)->capacity == 0) (da)->capacity = 256;         \
		if ((da)->count >= (da)->capacity - 1 || !(da)->items) \
			(da)->items = realloc((da)->items, sizeof(*(da)->items) * ((da)->capacity *= 2)); \
		(da)->items[(da)->count++] = (item);                   \
	} while (0)

void emit_ld(FILE *f, CodegenCtx *ctx, AST *dst, AST *src) {

}

Proc *get_proc(CodegenCtx *ctx, char *id) {
	for (size_t i = 0; i < ctx->proc.count; i++) {
		if (strcmp(ctx->proc.items[i].id, id) == 0) {
			return ctx->proc.items + i;
		}
	}
	return NULL;
}

Data *get_data(CodegenCtx *ctx, char *id) {
	for (size_t i = 0; i < ctx->data.count; i++) {
		if (strcmp(ctx->data.items[i].id, id) == 0) {
			return ctx->data.items + i;
		}
	}
	return NULL;
}

void emit_word(FILE *f, CodegenCtx *ctx, uint16_t word) {
	fputc((word >> 8) & 0xFF, f);
	fputc(word & 0xFF, f);
	ctx->addr += 2;
}

void emit_cond(FILE *f, CodegenCtx *ctx, AST *n) {
	// a > b
	//   vf = b
	//   SUB vf, a
	//   SE vf, 0
	//   JP .skip
	//   *body*
	//   JP .exit
	//   .skip:

	// a <= b
	//   vf = b
	//   SUB vf, a
	//   SE vf, 1
	//   JP .skip
	//   *body*
	//   JP .exit
	//   .skip:

	// a < b
	//   vf = b
	//   SUBN vf, a
	//   SE vf, 0
	//   JP .skip
	//   *body*
	//   JP .exit
	//   .skip:

	// a >= b
	//   vf = b
	//   SUBN vf, a
	//   SE vf, 1
	//   JP .skip
	//   *body*
	//   JP .exit
	//   .skip:
}

void emit_block(FILE *f, CodegenCtx *ctx, AST *n) {
	for (AST *c = n->as.block; c; c = c->next) {
		switch (c->kind) {
		case AST_MATH: {
			uint16_t inst = 0;

			AST_Opr lhs = c->as.math.lhs->as.opr;
			AST_Opr rhs = c->as.math.rhs->as.opr;

			if (lhs.kind == OPR_VREG && rhs.kind == OPR_VREG) {
				inst = 0x8000;

				inst |= (lhs.val & 0xF) << 8;
				inst |= (rhs.val & 0xF) << 4;

				switch (n->as.math.op) {
					case MOP_ADD: inst |= 0x4; break;
					case MOP_SUB: inst |= 0x5; break;
					case MOP_AND: inst |= 0x2; break;
					case MOP_OR:  inst |= 0x1; break;
					case MOP_XOR: inst |= 0x3; break;
				}
			} else if (
				lhs.kind == OPR_VREG &&
				rhs.kind == OPR_IMM &&
				c->as.math.op == MOP_ADD
			) {
				inst = 0x7000;

				inst |= (lhs.val & 0xF) << 8;
				inst |= rhs.val & 0xFF;
			}

			emit_word(f, ctx, inst);
		} break;

		case AST_IF:
			emit_block(f, ctx, c->as.ifs.block);
			break;

		case AST_WHILE:
			break;

		case AST_RETURN:
			fputc(0x00, f);
			fputc(0xEE, f);
			ctx->addr += 2;
		}
	}
}

void emit_bytes(FILE *f, CodegenCtx *ctx, AST *n) {
	Data data = {n->as.bytes.id, ctx->addr, n->as.bytes.count};
	da_append(&ctx->data, data);

	for (size_t i = 0; i < n->as.bytes.count; i++) {
		fputc(n->as.bytes.data[i], f);
		ctx->addr++;
	}
}

void emit_proc(FILE *f, CodegenCtx *ctx, AST *n) {
	Proc proc = {n->as.proc.id, ctx->addr};
	da_append(&ctx->proc, proc);
	ctx->cur_proc = &proc;
	emit_block(f, ctx, n->as.proc.block);
}

void emit_prog(FILE *f, AST *n) {
	CodegenCtx ctx = {.addr = 0x200};
	for (AST *c = n->as.prog; c; c = c->next) {
		switch (c->kind) {
			case AST_PROC:  emit_proc(f, &ctx, c);  break;
			case AST_BYTES: emit_bytes(f, &ctx, c); break;
		}
	}
}
