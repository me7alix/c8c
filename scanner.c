#include <stdio.h>
#include <stdlib.h>

typedef struct {
	char *file;
	unsigned line;
	unsigned col;
} Location;

typedef struct {
	struct {
		char *stream;
		Location loc;
	} stack[128];
	int depth;
} Scanner;

Location loc(Scanner *sc) {
	return sc->stack[sc->depth].loc;
}

char peek(Scanner *sc) {
	return *(sc->stack[sc->depth].stream);
}

char next(Scanner *sc) {
	if (peek(sc) == '\n') {
		sc->stack[sc->depth].loc.line++;
		sc->stack[sc->depth].loc.col = 0;
	} else {
		sc->stack[sc->depth].loc.col++;
	}

	char res = *(sc->stack[sc->depth].stream++);
	return res;
}

void skip_ws(Scanner *sc) {
	while (
		peek(sc) == ' '  ||
		peek(sc) == '%'  ||
		peek(sc) == '\t' ||
		peek(sc) == '\n'
	) {
		if (peek(sc) == '%') {
			next(sc);
			while (
				peek(sc) != '%' &&
				peek(sc) != '\0'
			) next(sc);
		}

		next(sc);
	}
}

Scanner *sc_from_file(char *filename) {
	FILE *f = fopen(filename, "r");
	if (!f) return NULL;

	if (fseek(f, 0L, SEEK_END) != 0) {
		fclose(f);
		return NULL;
	}

	long size = ftell(f);
	if (size < 0) {
		fclose(f);
		return NULL;
	}

	rewind(f);

	char *str = malloc((size_t)size + 1);
	if (!str) {
		fclose(f);
		return NULL;
	}

	size_t bytes_read = fread(str, 1, (size_t)size, f);

	if (bytes_read != (size_t)size) {
		free(str);
		fclose(f);
		return NULL;
	}

	str[bytes_read] = '\0';

	fclose(f);

	Scanner *sc = malloc(sizeof(*sc));
	sc->stack[0].stream = str;
	sc->stack[0].loc.file = filename;
	sc->stack[0].loc.line = 0;
	sc->stack[0].loc.col = 0;
	sc->depth = 0;

	return sc;
}

void sc_mark(void *self) {
	Scanner *mng = self;
	mng->stack[mng->depth + 1] = mng->stack[mng->depth];
	mng->depth++;
}

void sc_rewind(void *self) {
	Scanner *mng = self;
	mng->depth--;
}

void sc_unmark(void *self) {
	Scanner *mng = self;
	mng->stack[mng->depth - 1] = mng->stack[mng->depth];
	mng->depth--;
}
