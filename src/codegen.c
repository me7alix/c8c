#include <stdint.h>
#include <assert.h>
#include "transform.c"

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
	uint8_t *items;
	size_t count;
	size_t capacity;
} Bytes;

typedef struct {
	ProcCont proc;
	DataCont data;
	uint16_t addr;
	Bytes bytes;
	Proc *cur_proc;
} CodegenCtx;

static AST vf = {
	.kind = AST_OPR,
	.as.opr.kind = OPR_VREG,
	.as.opr.val = 0xF,
};

static AST ir = {
	.kind = AST_OPR,
	.as.opr.kind = OPR_IREG,
};

#define da_append(da, item)                                    \
	do {                                                       \
		if ((da)->capacity == 0) (da)->capacity = 256;         \
		if ((da)->count >= (da)->capacity - 1 || !(da)->items) \
			(da)->items = realloc((da)->items, sizeof(*(da)->items) * ((da)->capacity *= 2)); \
		(da)->items[(da)->count++] = (item);                   \
	} while (0)

void push_byte(CodegenCtx *ctx, uint8_t byte) {
	da_append(&ctx->bytes, byte);
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

void emit_word(CodegenCtx *ctx, uint16_t word) {
	push_byte(ctx, (word >> 8) & 0xFF);
	push_byte(ctx, word & 0xFF);
	ctx->addr += 2;
}

size_t emit_jump_placeholder(CodegenCtx *ctx) {
	emit_word(ctx, 0);
	return ctx->bytes.count - 2;
}

void emit_jump(CodegenCtx *ctx, uint16_t addr) {
	emit_word(ctx, 0x1000 | (addr & 0xFFF));
}

void patch_jump_address(CodegenCtx *ctx, size_t place, uint16_t addr) {
	uint16_t inst = 0x1000 | (addr & 0xFFF);
	ctx->bytes.items[place] = (inst >> 8) & 0xFF;
	ctx->bytes.items[place + 1] = inst & 0xFF;
}

void emit_ld(CodegenCtx *ctx, AST *dst, AST *src) {
	AST_Opr od = dst->as.opr;
	AST_Opr os = src->as.opr;

	if (od.kind == OPR_VREG && os.kind == OPR_VREG) {
		/* 8xy0 - LD Vx, Vy */
		uint16_t inst = 0x8000;
		inst |= (od.val) << 8;
		inst |= (os.val) << 4;
		emit_word(ctx, inst);
	} else if (od.kind == OPR_VREG && os.kind == OPR_IMM) {
		/* 6xkk - LD Vx, byte */
		uint16_t inst = 0x6000;
		inst |= (od.val) << 8;
		inst |= (os.val & 0xFF);
		emit_word(ctx, inst);
	} else if (od.kind == OPR_IREG && os.kind == OPR_IMM) {
		emit_word(ctx, 0xA000 | (os.val & 0xFFF));
	} else if (od.kind == OPR_DT && os.kind == OPR_VREG) {
		/* Fx15 - LD DT, Vx */
		emit_word(ctx, 0xF015 | ((os.val & 0xF) << 8));
	} else if (od.kind == OPR_DT && os.kind == OPR_IMM) {
		emit_ld(ctx, &vf, src);
		emit_word(ctx, 0xFF15);
	} else if (od.kind == OPR_VREG && os.kind == OPR_DT) {
		/* Fx07 - LD Vx, DT */
		emit_word(ctx, 0xF007 | ((od.val & 0xF) << 8));
	} else {
		throw_error(dst->loc, "operation is not supported");
	}
}

void emit_cond(CodegenCtx *ctx, AST *n) {
	AST *lhs = n->as.cond.lhs;
	AST *rhs = n->as.cond.rhs;

	if (lhs->kind != AST_OPR) {
		throw_error(lhs->loc, "operand expected");
	} else if (rhs->kind != AST_OPR) {
		throw_error(rhs->loc, "operand expected");
	}

	AST_Opr opl = lhs->as.opr;
	AST_Opr opr = rhs->as.opr;

	switch (n->as.cond.op) {
		case COP_EQ: {
			if (opl.kind == OPR_VREG && opr.kind == OPR_VREG) {
				/* 5xy0 - SE Vx, Vy */
				uint16_t inst = 0x5000;
				inst |= (opl.val & 0xF) << 8;
				inst |= (opr.val & 0xF) << 4;
				emit_word(ctx, inst);
			} else if (opl.kind == OPR_DT && opr.kind == OPR_VREG) {
				emit_ld(ctx, &vf, lhs);
				uint16_t inst = 0x5F00;
				inst |= (opr.val & 0xF) << 4;
				emit_word(ctx, inst);
			} else if (opl.kind == OPR_VREG && opr.kind == OPR_IMM) {
				/* 3xkk - SE Vx, byte */
				uint16_t inst = 0x3000;
				inst |= (opl.val & 0xF) << 8;
				inst |= opr.val & 0xFF;
				emit_word(ctx, inst);
			} else if (opl.kind == OPR_DT && opr.kind == OPR_IMM) {
				emit_ld(ctx, &vf, lhs);
				emit_word(ctx, 0x3F00 | (opr.val & 0xFF));
			} else {
				throw_error(n->loc, "operation is not supported");
			}
		} break;

		case COP_NOT_EQ: {
			if (opl.kind == OPR_VREG && opr.kind == OPR_VREG) {
				/* 9xy0 - SNE Vx, Vy */
				uint16_t inst = 0x9000;
				inst |= (opl.val & 0xF) << 8;
				inst |= (opr.val & 0xF) << 4;
				emit_word(ctx, inst);
			} else if (opl.kind == OPR_DT && opr.kind == OPR_VREG) {
				emit_ld(ctx, &vf, lhs);
				uint16_t inst = 0x9F00;
				inst |= (opr.val & 0xF) << 4;
				emit_word(ctx, inst);
			} else if (opl.kind == OPR_VREG && opr.kind == OPR_IMM) {
				/* 4xkk - SNE Vx, byte */
				uint16_t inst = 0x4000;
				inst |= (opl.val & 0xF) << 8;
				inst |= opr.val & 0xFF;
				emit_word(ctx, inst);
			} else if (opl.kind == OPR_DT && opr.kind == OPR_IMM) {
				emit_ld(ctx, &vf, lhs);
				emit_word(ctx, 0x4F00 | (opr.val & 0xFF));
			} else {
				throw_error(n->loc, "operation is not supported");
			}
		} break;

		case COP_GREATER: {
			// a > b
			//   LD vf, b
			//   SUB vf, a
			//   SE vf, 0

			emit_ld(ctx, &vf, rhs);
			if (lhs->kind != AST_OPR || lhs->as.opr.kind != OPR_VREG)
				throw_error(lhs->loc, "V register expected");
			emit_word(ctx, 0x8F05 | (((lhs->as.opr.val) & 0xF) << 4));
			emit_word(ctx, 0x3F00);
		} break;

		case COP_LESS: {
			// a < b
			//   LD vf, b
			//   SUBN vf, a
			//   SE vf, 0

			emit_ld(ctx, &vf, rhs);
			if (lhs->kind != AST_OPR || lhs->as.opr.kind != OPR_VREG)
				throw_error(lhs->loc, "V register expected");
			emit_word(ctx, 0x8F07 | (((lhs->as.opr.val) & 0xF) << 4));
			emit_word(ctx, 0x3F01);
		} break;

		case COP_GREATER_EQ: {
			// a >= b
			//   LD vf, b
			//   SUBN vf, a
			//   SE vf, 1

			emit_ld(ctx, &vf, rhs);
			if (lhs->kind != AST_OPR || lhs->as.opr.kind != OPR_VREG)
				throw_error(lhs->loc, "V register expected");
			emit_word(ctx, 0x8F07 | (((lhs->as.opr.val) & 0xF) << 4));
			emit_word(ctx, 0x3F00);
		} break;

		case COP_LESS_EQ: {
			// a <= b
			//   LD vf, b
			//   SUB vf, a
			//   SE vf, 1

			emit_ld(ctx, &vf, rhs);
			if (lhs->kind != AST_OPR || lhs->as.opr.kind != OPR_VREG)
				throw_error(lhs->loc, "V register expected");
			emit_word(ctx, 0x8F05 | (((lhs->as.opr.val) & 0xF) << 4));
			emit_word(ctx, 0x3F01);
		} break;
	}
}

void emit_block(CodegenCtx *ctx, AST *n);

void emit_ifs(CodegenCtx *ctx, AST *n) {
	const size_t max_size = 512;
	size_t jumps[max_size];
	size_t count = 0;
	size_t next_jump;

	while (n) {
		if (n->as.ifs.cond) {
			emit_cond(ctx, n->as.ifs.cond);
			next_jump = emit_jump_placeholder(ctx);
		}

		emit_block(ctx, n->as.ifs.block);

		if (n->as.ifs.next) {
			jumps[count++] = emit_jump_placeholder(ctx);
			assert(count <= max_size);
		}

		if (n->as.ifs.cond) {
			patch_jump_address(ctx, next_jump, ctx->addr);
		}

		n = n->as.ifs.next;
	}

	for (size_t i = 0; i < count; i++) {
		patch_jump_address(ctx, jumps[i], ctx->addr);
	}
}

void emit_block(CodegenCtx *ctx, AST *n) {
	for (AST *c = n->as.block; c; c = c->next) {
		switch (c->kind) {
		case AST_MATH: {
			uint16_t inst = 0;

			if (c->as.math.op == MOP_ASG && c->as.math.rhs->kind == AST_ID) {
				Data *dt = get_data(ctx, c->as.math.rhs->as.id);
				if (!dt) throw_error(c->as.math.rhs->loc, "no such data");
				emit_word(ctx, 0xA000 | (dt->addr & 0xFFF));
				break;
			}

			if (c->as.math.lhs->kind != AST_OPR) {
				throw_error(c->as.math.lhs->loc, "not an operand");
			} else if (c->as.math.rhs->kind != AST_OPR) {
				throw_error(c->as.math.rhs->loc, "not an operand");
			}

			AST_Opr lhs = c->as.math.lhs->as.opr;
			AST_Opr rhs = c->as.math.rhs->as.opr;

			if (c->as.math.op == MOP_ASG) {
				emit_ld(ctx, c->as.math.lhs, c->as.math.rhs);
				break;
			} else if (lhs.kind == OPR_VREG && rhs.kind == OPR_VREG) {
				inst = 0x8000;

				inst |= (lhs.val & 0xF) << 8;
				inst |= (rhs.val & 0xF) << 4;

				switch (c->as.math.op) {
					case MOP_ADD: inst |= 0x4; break;
					case MOP_SUB: inst |= 0x5; break;
					case MOP_AND: inst |= 0x2; break;
					case MOP_OR:  inst |= 0x1; break;
					case MOP_XOR: inst |= 0x3; break;
					default: throw_error(n->loc, "operation is not supported");
				}
			} else if (
				lhs.kind == OPR_VREG &&
				rhs.kind == OPR_IMM &&
				c->as.math.op == MOP_ADD
			) {
				inst = 0x7000;
				inst |= (lhs.val & 0xF) << 8;
				inst |= rhs.val & 0xFF;
			} else if (
				lhs.kind == OPR_VREG &&
				rhs.kind == OPR_IMM &&
				c->as.math.op == MOP_SUB
			) {
				emit_ld(ctx, &vf, c->as.math.rhs);
				inst = 0x80f5;
				inst |= (lhs.val & 0xF) << 8;
			} else if (lhs.kind == OPR_IREG && rhs.kind == OPR_VREG) {
				/* Fx1E - ADD I, Vx */
				inst = 0xF000;
				inst |= (rhs.val & 0xF) << 8;
				inst |= 0x1E;
			} else {
				throw_error(c->loc, "unsupported operation");
			}

			emit_word(ctx, inst);
		} break;

		case AST_CALL: {
			uint16_t inst = 0x2000;
			Proc *p = get_proc(ctx, c->as.call.id);
			if (!p) throw_error(c->loc, "no such procedure");
			inst |= p->addr & 0xFFF;
			emit_word(ctx, inst);
		} break;

		case AST_WHILE: {
			uint16_t beg_addr = ctx->addr;
			emit_cond(ctx, c->as.whiles.cond);
			size_t skip_jump = emit_jump_placeholder(ctx);
			emit_block(ctx, c->as.whiles.block);
			emit_jump(ctx, beg_addr);
			patch_jump_address(ctx, skip_jump, ctx->addr);
		} break;

		case AST_LOOP: {
			uint16_t beg_addr = ctx->addr;
			emit_block(ctx, c->as.loop.block);
			emit_jump(ctx, beg_addr);
		} break;

		case AST_IF:
			emit_ifs(ctx, c);
			break;

		case AST_RETURN:
			emit_word(ctx, 0x00EE);
			break;

		case AST_DRAW: {
			Data *dt = get_data(ctx, c->as.draw.data_id);
			if (!dt) throw_error(c->loc, "no such data (1st operand)");

			AST addr = {
				.kind = AST_OPR,
				.as.opr.kind = OPR_IMM,
				.as.opr.val = dt->addr,
			};

			emit_ld(ctx, &ir, &addr);

			uint16_t inst = 0xD000;
			inst |= (c->as.draw.x->as.opr.val & 0xF) << 8;
			inst |= (c->as.draw.y->as.opr.val & 0xF) << 4;
			inst |= dt->len & 0xF;
			emit_word(ctx, inst);
		} break;

		case AST_CLS:
			emit_word(ctx, 0x00E0);
			break;

		default:
			assert(0);
		}
	}
}

void emit_bytes(CodegenCtx *ctx, AST *n) {
	Data data = {n->as.bytes.id, ctx->addr, n->as.bytes.count};
	da_append(&ctx->data, data);

	for (size_t i = 0; i < n->as.bytes.count; i++) {
		push_byte(ctx, n->as.bytes.data[i]);
		ctx->addr++;
	}

	if (n->as.bytes.count % 2 != 0) {
		push_byte(ctx, 0);
		ctx->addr++;
	}
}

void emit_proc(CodegenCtx *ctx, AST *n) {
	Proc proc = {n->as.proc.id, ctx->addr};
	da_append(&ctx->proc, proc);
	ctx->cur_proc = &proc;
	emit_block(ctx, n->as.proc.block);
	emit_word(ctx, 0x00EE);
}

void emit_prog(FILE *f, AST *n) {
	CodegenCtx ctx = {.addr = 0x200};
	emit_word(&ctx, 0);

	for (AST *c = n->as.prog; c; c = c->next) {
		switch (c->kind) {
			case AST_PROC:  emit_proc(&ctx, c);  break;
			case AST_BYTES: emit_bytes(&ctx, c); break;
		}
	}

	Proc *main_proc = get_proc(&ctx, "main");
	if (!main_proc) {
		fprintf(stderr, "no main procedure\n");
		exit(1);
	}

	uint16_t call_main = 0x2000 | (main_proc->addr & 0xFFF);
	ctx.bytes.items[0] = (call_main >> 8) & 0xFF;
	ctx.bytes.items[1] = call_main & 0xFF;

	fwrite(
		ctx.bytes.items, 1,
		ctx.bytes.count, f
	);
}
