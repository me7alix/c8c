#include "codegen.c"

typedef struct {
	int argc;
	char **argv;
	int idx;
} ClParser;

char *cl_next(ClParser *p) {
	if (p->idx >= p->argc) return NULL;
	return p->argv[(p->idx)++];
}

char *cl_peek(ClParser *p) {
	if (p->idx >= p->argc) return NULL;
	return p->argv[p->idx];
}

bool cl_peek_eq(ClParser *p, char *str) {
	return strcmp(cl_peek(p), str) == 0;
}

void print_usage() {
	printf(
		"Usage: [options] file\n"
		"Options:\n"
		"  -fmt Format input file\n"
		"  -o   Output file path\n"
		"  -h   Show this message\n"
	);
}

int main(int argc, char *argv[]) {
	char *input = NULL;
	char *output = NULL;
	bool fmt = false;

	if (argc == 1) {
		print_usage();
		return 0;
	}

	ClParser p = {argc, argv};
	cl_next(&p);

	while (cl_peek(&p)) {
		if (cl_peek_eq(&p, "-fmt")) {
			fmt = true;
			cl_next(&p);
		} else if (cl_peek_eq(&p, "-o")) {
			cl_next(&p);
			output = cl_next(&p);
		} else if (cl_peek_eq(&p, "--help") || cl_peek_eq(&p, "-h")) {
			print_usage();
			return 0;
		} else if (!input) {
			input = cl_next(&p);
		} else {
			fprintf(stderr, "wrong command line argument\n");
			return 1;
		}
	}

	if (!input) {
		fprintf(stderr, "input file path must be provided\n");
		return 1;
	}

	if (!output && !fmt) {
		fprintf(stderr, "output file path must be provided\n");
		return 1;
	}

	SPC_Parser *pp = parser();

	Scanner *sc = sc_from_file(input);
	SPC_Input inp = {sc, sc_mark, sc_unmark, sc_rewind};

	SPC_Result res = pp->parse(pp, &inp);
	if (res.status != SPC_OK) {
		Error *err = res.data;
		throw_error(err->loc, err->message);
		return 1;
	}

	if (fmt) {
		FILE *f = output ? fopen(output, "wb") : stdout;
		if (f == NULL) {
			fprintf(stderr, "wrong output file path");
			return 1;
		}

		ast_dump(f, res.data, 4, 0);
	} else {
		FILE *f = fopen(output, "wb");
		if (f == NULL) {
			fprintf(stderr, "wrong input file path");
			return 1;
		}

		TransformCtx ctx = {0};
		transform_ast(&ctx, res.data);
		emit_prog(f, res.data);
	}

	return 0;
}
