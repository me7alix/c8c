#include <stdio.h>
#include <stdlib.h>

#define peek(l) (*((l)->stack[(l)->depth].stream))
#define next(l) (*((l)->stack[(l)->depth].stream++))
#define loc(l)  ((l)->stack[(l)->depth].loc)
#define skip_ws(l) while (peek(l) == ' ' || peek(l) == '\t' || peek(l) == '\n') next(l)

typedef struct {
	char *file;
	char *line_beg;
	unsigned line_num;
	char *elem_beg;
} Location;

typedef struct {
	struct {
		char *stream;
		Location loc;
	} stack[128];
	int depth;
} Scanner;

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
