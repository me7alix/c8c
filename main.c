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

int main(int argc, char *argv[]) {
	char *input = NULL;
	char *output = NULL;

	ClParser p = {argc, argv};
	cl_next(&p);

	while (cl_peek(&p)) {
		if (!input) {
			input = cl_next(&p);
		} else {
			output = cl_next(&p);
		}
	}

	SPC_Parser *pp = parser();

	Scanner *sc = sc_from_file(input);
	SPC_Input inp = {sc, sc_mark, sc_unmark, sc_rewind};

	SPC_Result res = pp->parse(pp, &inp);
	if (res.status != SPC_OK) {
		Error *err = res.data;
		printf(
			"%s:%u:%u error: %s\n",
			err->loc.file,
			err->loc.line + 1,
			err->loc.col + 1,
			err->message);
		return 1;
	}

	ast_dump(stdout, res.data, 4, 0);

	FILE *f = fopen(output, "wb");
	emit_prog(f, res.data);

	return 0;
}
