/*
 * Copyright (c) 2021 Brian Callahan <bcallah@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BIN_MAX	0xff00

/*
 * Struct holding name and matching calculating addresses.
 */
typedef struct collection
{
	char *symbol;
	unsigned int addr;
	struct collection *next;
} collection_t;
static collection_t *head;

/*
 * Error handling.
 */
static int
error(const char *fmt, ...)
{
	va_list ap;

	(void) fputs("ld: error: ", stderr);

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);

	(void) fputc('\n', stderr);

	return 1;
}

/*
 * Pass 1: Collect symbol names.
 */
static int
collect1(int argc, char *argv[])
{
	FILE *fp;
	collection_t *curr, *new;
	char symbol[16];
	int ch, i, j;
	unsigned long addr = 0x100;

	for (i = 2; i < argc; i++) {
		if ((fp = fopen(argv[i], "r")) == NULL)
			return error("could not open input file: %s", argv[i]);

		while ((ch = fgetc(fp)) != EOF) {
			switch (ch) {
			case '\0':
				if ((ch = fgetc(fp)) == EOF)
					return error("invalid data byte");

				/* Increment address counter.  */
				if (++addr > USHRT_MAX)
					return error("final binary exceeds 65,280 bytes");

				break;
			case '\001':
				/* Get symbol name.  */
				j = 0;

				(void) memset(symbol, 0, sizeof(symbol));

				while ((ch = fgetc(fp)) != '\001') {
					if (ch == EOF)
						return error("unterminated symbol");

					if (j < sizeof(symbol) - 1)
						symbol[j++] = ch;
				}

				/* Make sure there is actually a symbol here.  */
				if (strlen(symbol) == 0)
					return error("empty symbol");

				/* Make sure this symbol isn't a duplicate.  */
				curr = head;
				while (curr != NULL) {
					if (!strcmp(symbol, curr->symbol))
						return error("duplicate symbol: %s", symbol);

					curr = curr->next;
				}

				/* Put symbol in collection.  */
				curr = head;
				while (curr->next != NULL)
					curr = curr->next;

				if ((new = malloc(sizeof(collection_t))) == NULL)
					return error("could not create symbol entry");

				new->symbol = strdup(symbol);
				new->addr = addr;
				new->next = NULL;

				curr->next = new;

				break;
			case '\002':
				/* Ignore references during pass 1.  */
				while ((ch = fgetc(fp)) != '\002') {
					if (ch == EOF)
						return error("unterminated symbol");
				}

				/* But still increment address counter.  */
				addr += 2;
				if (addr > USHRT_MAX)
					return error("final binary exceeds 65,280 bytes");

				break;
			case '\003':
				/* Do nothing for now.  */
				break;
			case '\032':
				/* CP/M EOF.  */
				return 0;
			default:
				/* This should never happen.  */
				return error("unknown control byte: %d", ch);
			}
		}

		(void) fclose(fp);
	}

	return 0;
}

static int
process2(int argc, char *argv[], FILE *fq)
{
	FILE *fp;
	collection_t *curr;
	char symbol[16];
	int ch, found, i, j;
	unsigned int bin = 0;

	for (i = 2; i < argc; i++) {
		if ((fp = fopen(argv[i], "r")) == NULL)
			return error("could not open input file: %s", argv[i]);

		while ((ch = fgetc(fp)) != EOF) {
			switch (ch) {
			case '\0':
				/* Write out data byte.  */
				if ((ch = fgetc(fp)) == EOF)
					return error("invalid data byte");

				(void) fputc(ch, fq);
				if (bin++ == BIN_MAX)
					return error("binary too large");

				break;
			case '\001':
				/* Ignore declarations during pass 2.  */
				while ((ch = fgetc(fp)) != '\001') {
					if (ch == EOF)
						return error("unterminated delcaration");
				}

				break;
			case '\002':
				/* Get and write out symbol reference.  */
				curr = head;
				found = 0;
				j = 0;

				(void) memset(symbol, 0, sizeof(symbol));
				while ((ch = fgetc(fp)) != '\002') {
					if (ch == EOF)
						return error("unterminated reference");

					if (j < sizeof(symbol) - 1)
						symbol[j++] = ch;
				}

				while (curr != NULL) {
					if (!strcmp(symbol, curr->symbol)) {
						found = 1;
						break;
					}

					curr = curr->next;
				}

				if (found == 0)
					return error("undefined reference: %s", symbol);

				(void) fputc(curr->addr & 0xff, fq);
				if (bin++ == BIN_MAX)
					return error("binary too large");

				(void) fputc((curr->addr >> 8) & 0xff, fq);
				if (bin++ == BIN_MAX)
					return error("binary too large");

				break;
			case '\003':
				/* Do nothing for now.  */
				break;
			case '\032':
				/* CP/M EOF.  */
				return 0;
			default:
				/* This should never happen.  */
				return error("unknown control byte: %d", ch);
			}
		}

		(void) fclose(fp);
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	collection_t *curr;
	char *ending;
	char bin[13];	/* 8 + 3 + DOT */
	int i, ret;

	/* Make sure we have the correct number of arguments.  */
	if (argc < 3) {
		fprintf(stderr, "usage: ld binary file1.obj [file2.obj ...]\n");
		return 1;
	}

	/* Check endings of all input files.  */
	for (i = 2; i < argc; i++) {
		ending = strrchr(argv[i], '.');
		if (strcmp(ending, ".obj") && strcmp(ending, ".OBJ")) {
			if (strcmp(ending, ".lib") && strcmp(ending, ".LIB"))
				return error("input files must end in \".obj\" or \".lib\"");
		}
	}

	(void) memset(bin, 0, sizeof(bin));

	(void) strncpy(bin, argv[1], sizeof(bin) - 1);
	if (strlen(bin) > 8)
		return error("output name \"%s.com\" too long", argv[1]);

	(void) strncat(bin, ".com", sizeof(bin) - 1);

	/* Preload collection with initial empty symbol.  */
	if ((curr = malloc(sizeof(collection_t))) == NULL)
		return error("could not add symbol");

	curr->symbol = "@";
	curr->addr = 0;
	curr->next = NULL;

	head = curr;

	if ((fp = fopen(bin, "w+")) == NULL)
		return error("could not open output file: %s", bin);

	if ((ret = collect1(argc, argv)) == 0)
		ret = process2(argc, argv, fp);

	(void) fclose(fp);

	if (ret != 0)
		(void) unlink(bin);

	return ret;
}
