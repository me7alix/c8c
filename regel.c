/*
 * RELEX
 * universal lexer based on regex
 */

#ifndef RELEX_H_
#define RELEX_H_

#include <regex.h>
#include "spc.h"

typedef struct {
	size_t line;
	size_t column;
} RLX_Location;

typedef struct {
	struct {
		char *stream;
		RLX_Location loc;
	} stack[128];
	int depth;
} RLX_Scanner;

void rlx_mark(void *self) {
}

void rlx_unmark(void *self) {
}

void rlx_rewind(void *self) {
}

typedef struct {
	regex_t
} RLX_ParseRegex;

#endif // REGEL_H_
