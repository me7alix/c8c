#include <stdio.h>

typedef struct {
	char *file;
	unsigned line;
	unsigned col;
} Location;

void throw_error(Location loc, char *msg) {
	FILE *f = fopen(loc.file, "r");
	char buf[1024];
	size_t count = 0;

    while (fgets(buf, sizeof buf, f) != NULL) {
		if (count++ == loc.line) {
			fprintf(stderr,
				"%s:%u:%u error: %s\n",
				loc.file,
				loc.line + 1,
				loc.col + 1,
				msg);
			fprintf(stderr, "%s", buf);
			fclose(f);
			exit(1);
		}
	}
}
