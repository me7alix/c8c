#ifndef SPC_H_
#define SPC_H_

#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

typedef enum {
	SPC_OK,
	SPC_FAIL,
	SPC_FATAL
} SPC_Status;

typedef void (*SPC_FreeFn)(void*);

typedef struct {
	SPC_Status status;
	void *data;
	SPC_FreeFn destroy;
} SPC_Result;

typedef struct {
	void *self;
	void (*mark)(void*);
	void (*unmark)(void*);
	void (*rewind)(void*);
} SPC_Input;

typedef struct SPC_Parser SPC_Parser;
typedef SPC_Result (*SPC_ParseFn)(SPC_Parser*, SPC_Input*);
typedef SPC_Result (*SPC_FoldFn)(int n, void**);
typedef SPC_Result (*SPC_MapFn)(void*);


struct SPC_Parser {
	SPC_ParseFn parse;
	void *ctx;
};

SPC_Parser *spc_map(SPC_MapFn transform, SPC_Parser *parser);
SPC_Parser *spc_choice(int n, ...);
SPC_Parser *spc_sequence(SPC_FoldFn fold, int n, ...);
SPC_Parser *spc_keep_right(SPC_Parser *left, SPC_Parser *right);
SPC_Parser *spc_keep_left(SPC_Parser *left, SPC_Parser *right);
SPC_Parser *spc_between(SPC_Parser *left, SPC_Parser *middle, SPC_Parser *right);
SPC_Parser *spc_lazy(SPC_Parser **ref);
SPC_Parser *spc_chain_left(SPC_FoldFn fold, SPC_Parser *operand, SPC_Parser *operator);
SPC_Parser *spc_chain_right(SPC_FoldFn fold, SPC_Parser *operand, SPC_Parser *operator);
SPC_Parser *spc_sep_by(SPC_FoldFn fold, SPC_Parser *item, SPC_Parser *separator);
SPC_Parser *spc_sep_by1(SPC_FoldFn fold, SPC_Parser *item, SPC_Parser *separator);
SPC_Parser *spc_many(SPC_FoldFn fold, SPC_Parser *item);
SPC_Parser *spc_many1(SPC_FoldFn fold, SPC_Parser *item);
SPC_Parser *spc_optional(SPC_Parser *parser);
SPC_Parser *spc_commit(SPC_Parser *parser);
SPC_Parser *spc_try(SPC_Parser *parser);

static inline SPC_Result spc_ok(void *data, SPC_FreeFn ff) {
	return (SPC_Result){SPC_OK, data, ff};
}

static inline SPC_Result spc_fail(void *error, SPC_FreeFn ff) {
	return (SPC_Result){SPC_FAIL, error, ff};
}

static inline SPC_Result spc_fatal(void *error, SPC_FreeFn ff) {
	return (SPC_Result){SPC_FATAL, error, ff};
}

#endif // SPC_H_

#ifdef SPC_IMPLEMENTATION

#define SPC_FREE_DATA(res) \
	do { if ((res).destroy) (res).destroy((res).data); } while (0)

typedef struct {
	SPC_Parser *parser;
	SPC_MapFn transform;
} SPC_MapCtx;

SPC_Result spc_parse_map(SPC_Parser *self, SPC_Input *input) {
	SPC_MapCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result res = ctx->parser->parse(ctx->parser, input);
	if (res.status == SPC_FAIL) {
		input->rewind(input->self);
		return res;
	} else if (res.status == SPC_FATAL) {
		input->unmark(input->self);
		return res;
	}

	input->unmark(input->self);

	return ctx->transform(res.data);
}

SPC_Parser *spc_map(SPC_MapFn transform, SPC_Parser *parser) {
	SPC_MapCtx *ctx = malloc(sizeof(*ctx));
	ctx->parser = parser;
	ctx->transform = transform;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_map;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser **items;
	int count;
} SPC_ChoiceCtx;

SPC_Result spc_parse_choice(SPC_Parser *self, SPC_Input *input) {
	SPC_ChoiceCtx *ctx = self->ctx;
	SPC_Result res = {0};

	for (int i = 0; i < ctx->count; i++) {
		input->mark(input->self);

		res = ctx->items[i]->parse(ctx->items[i], input);
		if (res.status == SPC_OK || res.status == SPC_FATAL) {
			input->unmark(input->self);
			return res;
		}

		input->rewind(input->self);

		if (i < ctx->count - 1) {
			SPC_FREE_DATA(res);
		}
	}

	return res;
}

SPC_Parser *spc_choice(int n, ...) {
	SPC_ChoiceCtx *ctx = malloc(sizeof(*ctx));
	ctx->items = malloc(sizeof(void*) * n);
	ctx->count = n;

	va_list ap;
	va_start(ap, n);
	for (int i = 0; i < n; i++)
		ctx->items[i] = va_arg(ap, SPC_Parser*);
	va_end(ap);

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_choice;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser **items;
	int count;
	SPC_FoldFn fold;
} SPC_SequenceCtx;

SPC_Result spc_parse_sequence(SPC_Parser *self, SPC_Input *input) {
	SPC_SequenceCtx *ctx = self->ctx;
	void *items[ctx->count];
	SPC_FreeFn ffs[ctx->count];

	input->mark(input->self);

	for (int i = 0; i < ctx->count; i++) {
		SPC_Result res = ctx->items[i]->parse(ctx->items[i], input);
		if (res.status != SPC_OK) {
			for (int j = 0; j < i; j++) {
				if (ffs[j]) ffs[j](items[j]);
			}

			if (i > 0 || res.status == SPC_FATAL) {
				input->unmark(input->self);
				res.status = SPC_FATAL;
				return res;
			}

			input->rewind(input->self);

			return res;
		}

		items[i] = res.data;
		ffs[i] = res.destroy;
	}

	input->unmark(input->self);

	return ctx->fold(ctx->count, items);
}

SPC_Parser *spc_sequence(SPC_FoldFn fold, int n, ...) {
	SPC_SequenceCtx *ctx = malloc(sizeof(*ctx));
	ctx->fold = fold;
	ctx->items = malloc(sizeof(void*) * n);
	ctx->count = n;

	va_list ap;
	va_start(ap, n);
	for (int i = 0; i < n; i++)
		ctx->items[i] = va_arg(ap, SPC_Parser*);
	va_end(ap);

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_sequence;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser *left;
	SPC_Parser *right;
} SPC_KeepRightCtx;

SPC_Result parse_keep_right(SPC_Parser *self, SPC_Input *input) {
	SPC_KeepRightCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result left = ctx->left->parse(ctx->left, input);
	if (left.status == SPC_FAIL) {
		input->rewind(input->self);
		return left;
	} else if (left.status == SPC_FATAL) {
		input->unmark(input->self);
		return left;
	}

	SPC_FREE_DATA(left);

	SPC_Result right = ctx->right->parse(ctx->right, input);
	if (right.status != SPC_OK) {
		input->unmark(input->self);
		right.status = SPC_FATAL;
		return right;
	}

	input->unmark(input->self);

	return spc_ok(right.data, right.destroy);
}

SPC_Parser *spc_keep_right(SPC_Parser *left, SPC_Parser *right) {
	SPC_KeepRightCtx *ctx = malloc(sizeof(*ctx));
	ctx->left = left;
	ctx->right = right;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = parse_keep_right;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser *left;
	SPC_Parser *right;
} KeepLeftCtx;

SPC_Result spc_parse_keep_left(SPC_Parser *self, SPC_Input *input) {
	KeepLeftCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result left = ctx->left->parse(ctx->left, input);
	if (left.status == SPC_FAIL) {
		input->rewind(input->self);
		return left;
	} else if (left.status == SPC_FATAL) {
		input->unmark(input->self);
		return left;
	}

	SPC_Result right = ctx->right->parse(ctx->right, input);
	if (right.status != SPC_OK) {
		SPC_FREE_DATA(left);
		input->unmark(input->self);
		right.status = SPC_FATAL;
		return right;
	}

	SPC_FREE_DATA(right);
	input->unmark(input->self);

	return spc_ok(left.data, left.destroy);
}

SPC_Parser *spc_keep_left(SPC_Parser *left, SPC_Parser *right) {
	KeepLeftCtx *ctx = malloc(sizeof(*ctx));
	ctx->left = left;
	ctx->right = right;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_keep_left;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser *left;
	SPC_Parser *middle;
	SPC_Parser *right;
} SPC_BetweenCtx;

SPC_Result spc_parse_between(SPC_Parser *self, SPC_Input *input) {
	SPC_BetweenCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result left = ctx->left->parse(ctx->left, input);
	if (left.status == SPC_FAIL) {
		input->rewind(input->self);
		return left;
	} else if (left.status == SPC_FATAL) {
		input->unmark(input->self);
		return left;
	}

	SPC_FREE_DATA(left);

	SPC_Result middle = ctx->middle->parse(ctx->middle, input);
	if (middle.status != SPC_OK) {
		input->unmark(input->self);
		middle.status = SPC_FATAL;
		return middle;
	}

	SPC_Result right = ctx->right->parse(ctx->right, input);
	if (right.status != SPC_OK) {
		SPC_FREE_DATA(middle);
		input->unmark(input->self);
		right.status = SPC_FATAL;
		return right;
	}

	SPC_FREE_DATA(right);
	input->unmark(input->self);

	return spc_ok(middle.data, middle.destroy);
}

SPC_Parser *spc_between(SPC_Parser *left, SPC_Parser *middle, SPC_Parser *right) {
	SPC_BetweenCtx *ctx = malloc(sizeof(*ctx));
	ctx->left = left;
	ctx->middle = middle;
	ctx->right = right;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_between;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser **ref;
} SPC_LazyCtx;

SPC_Result spc_parse_lazy(SPC_Parser *self, SPC_Input *input) {
	SPC_LazyCtx *ctx = self->ctx;
	SPC_Parser *actual = *(ctx->ref);
	return actual->parse(actual, input);
}

SPC_Parser *spc_lazy(SPC_Parser **ref) {
	SPC_Parser *p = malloc(sizeof(*p));
	SPC_LazyCtx *ctx = malloc(sizeof(*ctx));
	ctx->ref = ref;

	p->parse = spc_parse_lazy;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser *operand;
	SPC_Parser *operator;
	SPC_FoldFn fold;
} SPC_ChainLeftCtx;

SPC_Result spc_parse_chain_left(SPC_Parser *self, SPC_Input *input) {
	SPC_ChainLeftCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result left = ctx->operand->parse(ctx->operand, input);
	if (left.status == SPC_FAIL) {
		input->rewind(input->self);
		return left;
	} else if (left.status == SPC_FATAL) {
		input->unmark(input->self);
		return left;
	}

	input->unmark(input->self);

	while (true) {
		input->mark(input->self);

		SPC_Result middle = ctx->operator->parse(ctx->operator, input);
		if (middle.status == SPC_FAIL) {
			SPC_FREE_DATA(middle);
			input->rewind(input->self);
			break;
		} else if (middle.status == SPC_FATAL) {
			SPC_FREE_DATA(left);
			input->unmark(input->self);
			return middle;
		}

		SPC_Result right = ctx->operand->parse(ctx->operand, input);
		if (right.status != SPC_OK) {
			SPC_FREE_DATA(left);
			SPC_FREE_DATA(middle);
			input->unmark(input->self);
			right.status = SPC_FATAL;
			return right;
		}

		void *arr[] = {left.data, middle.data, right.data};
		left = ctx->fold(3, arr);

		input->unmark(input->self);
	}

	return left;
}

SPC_Parser *spc_chain_left(SPC_FoldFn fold, SPC_Parser *operand, SPC_Parser *operator) {
	SPC_ChainLeftCtx *ctx = malloc(sizeof(*ctx));
	ctx->operand = operand;
	ctx->operator = operator;
	ctx->fold = fold;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_chain_left;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser *operand;
	SPC_Parser *operator;
	SPC_FoldFn fold;
} SPC_ChainRightCtx;

SPC_Result spc_parse_chain_right(SPC_Parser *self, SPC_Input *input) {
	SPC_ChainRightCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result left = ctx->operand->parse(ctx->operand, input);
	if (left.status == SPC_FAIL) {
		input->rewind(input->self);
		return left;
	} else if (left.status == SPC_FATAL) {
		input->unmark(input->self);
		return left;
	}

	input->unmark(input->self);

	input->mark(input->self);

	SPC_Result middle = ctx->operator->parse(ctx->operator, input);
	if (middle.status == SPC_FAIL) {
		SPC_FREE_DATA(middle);
		input->rewind(input->self);
		return left;
	} else if (middle.status == SPC_FATAL) {
		SPC_FREE_DATA(left);
		input->unmark(input->self);
		return middle;
	}

	SPC_Parser req_parser = {spc_parse_chain_right, ctx};
	SPC_Result right = req_parser.parse(&req_parser, input);

	if (right.status != SPC_OK) {
		SPC_FREE_DATA(left);
		SPC_FREE_DATA(middle);

		input->unmark(input->self);
		right.status = SPC_FATAL;
		return right;
	}

	input->unmark(input->self);

	void *arr[] = {left.data, middle.data, right.data};
	return ctx->fold(3, arr);
}

SPC_Parser *spc_chain_right(SPC_FoldFn fold, SPC_Parser *operand, SPC_Parser *operator) {
	SPC_ChainRightCtx *ctx = malloc(sizeof(*ctx));
	ctx->operand = operand;
	ctx->operator = operator;
	ctx->fold = fold;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_chain_right;
	p->ctx = ctx;
	return p;
}

typedef struct {
	void **items;
	size_t count;
	size_t capacity;
} SPC_DynArr;

void spc_dynarr_append(SPC_DynArr *da, void *item) {
	if (da->capacity == 0) da->capacity = 128;
	if (da->count >= da->capacity - 1 || !da->items)
		da->items = realloc(da->items, sizeof(void*) * (da->capacity *= 2));
	da->items[da->count++] = item;
}

typedef struct {
	SPC_Parser *item;
	SPC_Parser *separator;
	SPC_FoldFn fold;
	bool at_least_one;
} SPC_SepByCtx;

void spc_dynarr_free(SPC_DynArr *da) {
	if (da->items) {
		free(da->items);
		da->items = NULL;
	}
}

void spc_free_all(SPC_DynArr *arr, SPC_DynArr *ffs) {
	for (size_t i = 0; i < arr->count; i++) {
		SPC_FreeFn fn = (SPC_FreeFn)ffs->items[i];
		if (fn) fn(arr->items[i]);
	}

	spc_dynarr_free(arr);
	spc_dynarr_free(ffs);
}

SPC_Result spc_parse_sep_by(SPC_Parser *self, SPC_Input *input) {
	SPC_SepByCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result first = ctx->item->parse(ctx->item, input);
	if (first.status != SPC_OK) {
		if (first.status == SPC_FAIL) {
			input->rewind(input->self);
		} else {
			input->unmark(input->self);
		}

		if (ctx->at_least_one || first.status == SPC_FATAL) {
			return first;
		}

		SPC_FREE_DATA(first);

		return ctx->fold(0, NULL);
	}

	input->unmark(input->self);

	SPC_DynArr arr = {0};
	SPC_DynArr ffs = {0};

	spc_dynarr_append(&arr, first.data);
	spc_dynarr_append(&ffs, (void*)first.destroy);

	while (true) {
		input->mark(input->self);

		SPC_Result sep = ctx->separator->parse(ctx->separator, input);
		if (sep.status == SPC_FAIL) {
			SPC_FREE_DATA(sep);
			break;
		} else if (sep.status == SPC_FATAL) {
			input->unmark(input->self);
			spc_free_all(&arr, &ffs);
			return sep;
		}

		SPC_FREE_DATA(sep);

		SPC_Result next = ctx->item->parse(ctx->item, input);
		if (next.status != SPC_OK) {
			input->unmark(input->self);
			spc_free_all(&arr, &ffs);
			next.status = SPC_FATAL;
			return next;
		}

		spc_dynarr_append(&arr, next.data);
		spc_dynarr_append(&ffs, (void*)next.destroy);

		input->unmark(input->self);
	}

	input->rewind(input->self);

	SPC_Result res = ctx->fold(arr.count, arr.items);

	spc_dynarr_free(&arr);
	spc_dynarr_free(&ffs);

	return res;
}

SPC_Parser *spc_sep_by(SPC_FoldFn fold, SPC_Parser *item, SPC_Parser *separator) {
	SPC_SepByCtx *ctx = malloc(sizeof(*ctx));
	ctx->item = item;
	ctx->separator = separator;
	ctx->at_least_one = false;
	ctx->fold = fold;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_sep_by;
	p->ctx = ctx;
	return p;
}

SPC_Parser *spc_sep_by1(SPC_FoldFn fold, SPC_Parser *item, SPC_Parser *separator) {
	SPC_SepByCtx *ctx = malloc(sizeof(*ctx));
	ctx->item = item;
	ctx->separator = separator;
	ctx->at_least_one = true;
	ctx->fold = fold;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_sep_by;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser *item;
	SPC_FoldFn fold;
	bool at_least_one;
} SPC_ManyCtx;

SPC_Result spc_parse_many(SPC_Parser *self, SPC_Input *input) {
	SPC_ManyCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result first = ctx->item->parse(ctx->item, input);
	if (first.status != SPC_OK) {
		if (first.status == SPC_FAIL) {
			input->rewind(input->self);
		} else {
			input->unmark(input->self);
		}

		if (ctx->at_least_one || first.status == SPC_FATAL) {
			return first;
		}

		SPC_FREE_DATA(first);

		return ctx->fold(0, NULL);
	}

	input->unmark(input->self);

	SPC_DynArr arr = {0};
	SPC_DynArr ffs = {0};
	spc_dynarr_append(&arr, first.data);
	spc_dynarr_append(&ffs, (void*)first.destroy);

	while (true) {
		input->mark(input->self);

		SPC_Result item = ctx->item->parse(ctx->item, input);
		if (item.status == SPC_FAIL) {
			SPC_FREE_DATA(item);
			break;
		} else if (item.status == SPC_FATAL) {
			input->unmark(input->self);
			spc_free_all(&arr, &ffs);
			return item;
		}

		spc_dynarr_append(&arr, item.data);
		spc_dynarr_append(&ffs, (void*)item.destroy);

		input->unmark(input->self);
	}

	input->rewind(input->self);

	SPC_Result res = ctx->fold(arr.count, arr.items);

	spc_dynarr_free(&arr);
	spc_dynarr_free(&ffs);

	return res;
}

SPC_Parser *spc_many(SPC_FoldFn fold, SPC_Parser *item) {
	SPC_ManyCtx *ctx = malloc(sizeof(*ctx));
	ctx->item = item;
	ctx->fold = fold;
	ctx->at_least_one = false;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_many;
	p->ctx = ctx;
	return p;
}

SPC_Parser *spc_many1(SPC_FoldFn fold, SPC_Parser *item) {
	SPC_ManyCtx *ctx = malloc(sizeof(*ctx));
	ctx->item = item;
	ctx->fold = fold;
	ctx->at_least_one = true;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_many;
	p->ctx = ctx;
	return p;
}

typedef struct {
	SPC_Parser *parser;
} SPC_UnaryCtx;

SPC_Result spc_parse_optional(SPC_Parser *self, SPC_Input *input) {
	SPC_UnaryCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result res = ctx->parser->parse(ctx->parser, input);
	if (res.status == SPC_FAIL) {
		SPC_FREE_DATA(res);
		input->rewind(input->self);
		return spc_ok(NULL, NULL);
	}

	input->unmark(input->self);
	return res;
}

SPC_Parser *spc_optional(SPC_Parser *parser) {
	SPC_UnaryCtx *ctx = malloc(sizeof(*ctx));
	ctx->parser = parser;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_optional;
	p->ctx = ctx;
	return p;
}

SPC_Result spc_parse_commit(SPC_Parser *self, SPC_Input *input) {
	SPC_UnaryCtx *ctx = self->ctx;

	SPC_Result res = ctx->parser->parse(ctx->parser, input);
	if (res.status == SPC_FAIL) {
		res.status = SPC_FATAL;
	}

	return res;
}

SPC_Parser *spc_commit(SPC_Parser *parser) {
	SPC_UnaryCtx *ctx = malloc(sizeof(*ctx));
	ctx->parser = parser;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_commit;
	p->ctx = ctx;
	return p;
}

SPC_Result spc_parse_try(SPC_Parser *self, SPC_Input *input) {
	SPC_UnaryCtx *ctx = self->ctx;

	input->mark(input->self);

	SPC_Result res = ctx->parser->parse(ctx->parser, input);
	if (res.status != SPC_OK) {
		input->rewind(input->self);
		res.status = SPC_FAIL;
		return res;
	}

	input->unmark(input->self);
	return res;
}

SPC_Parser *spc_try(SPC_Parser *parser) {
	SPC_UnaryCtx *ctx = malloc(sizeof(*ctx));
	ctx->parser = parser;

	SPC_Parser *p = malloc(sizeof(*p));
	p->parse = spc_parse_try;
	p->ctx = ctx;
	return p;
}

#endif // SPC_IMPLEMENTATION
