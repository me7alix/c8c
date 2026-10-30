#include "codegen.c"

int main(int argc, char *argv[]) {
	SPC_Parser *p = parser();

	Scanner *sc = sc_from_file(argv[1]);
	SPC_Input inp = {sc, sc_mark, sc_unmark, sc_rewind};

	SPC_Result res = p->parse(p, &inp);
	if (res.status != SPC_OK) {
		printf("error: %s\n", ((Error*)res.data)->message);
		return 1;
	}

	ast_dump(stdout, res.data, 4, 0);
	return 0;
}
