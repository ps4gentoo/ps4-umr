/*
 * Copyright 2021 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Tom St Denis <tom.stdenis@amd.com>
 *
 *
 *
 * This program compiles offset and sh_mask headers into a text format to be used by umr.
 */
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#if defined(__unix__)
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
#endif

#define MAXLEN 256

enum regtype {
	REG_MMIO,
	REG_DIDT,
	REG_SMC,
	REG_PCIE,
	REG_SMN,
	REG_ERROR
};

struct bitfield {
	uint64_t shift, mask;
	int start, stop;
	char name[MAXLEN];
	struct bitfield *next;
};

struct regs {
	enum regtype type;
	uint64_t addr;
	char name[MAXLEN];
	struct bitfield *bits;
	uint32_t nobits, is64, idx;
	struct regs *next;
};

/* skip whitespace */
void whitespace(char **p)
{
	while (**p && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r'))
		++(*p);
}

/* seek until we hit a #define */
int define(char **p)
{
	int r;
	whitespace(p);
	while (**p && memcmp(*p, "#define", 7)) {
		// skip to end of line
		while (**p && (**p != '\n')) {
			++(*p);
		}
		whitespace(p);
	}
	r = memcmp(*p, "#define", 7);
	if (!r) (*p) += 7;
	return r;
}

uint64_t number(char **p)
{
	uint64_t v = 0;

	whitespace(p);
	if (**p && sscanf(*p, "0x%" SCNx64, &v) != 1) {
		sscanf(*p, "%" SCNu64, &v);
	}

	// skip over number
	while (**p && (isxdigit(**p)))
		++(*p);

	return v;
}

void symbol(char **p, char *dest)
{
	whitespace(p);
	while (**p && (isalnum(**p) || **p == '_')) {
		*dest++ = **p;
		++(*p);
	}
	*dest = 0;
}

// find a register with a full name (with mm/reg/etc)
// the bit header doesn't have the mm/reg/etc prefix
struct regs *find_reg(struct regs *or, struct regs *r, char *name)
{
	int try = 0;
	struct regs *pr = r;
retry:
	while (r) {
		if (try && pr == r)
			return NULL;
		if (!memcmp(r->name, "mm", 2) && !strcmp(r->name + 2, name))
			break;
		if (!memcmp(r->name, "ix", 2) && !strcmp(r->name + 2, name))
			break;
		if (!memcmp(r->name, "reg", 3) && !strcmp(r->name + 3, name))
			break;
		if (!memcmp(r->name, "cfg", 3) && !strcmp(r->name + 3, name))
			break;
		if (!memcmp(r->name, "smn", 3) && !strcmp(r->name + 3, name))
			break;
		r = r->next;
	}

	// retry from start
	if (!r && !try) {
		try = 1;
		r = or;
		goto retry;
	}
	return r;
}

struct bitfield *find_bit(struct regs *fr, char *name)
{
	struct bitfield *bf;
	bf = fr->bits;
	while (bf) {
		if (!strcmp(bf->name, name))
			break;
		bf = bf->next;
	}
	return bf;
}

struct bitfield *add_bit(struct regs *fr, char *name)
{
	struct bitfield **pbf;
	pbf = &fr->bits;
	while (*pbf) {
		pbf = &((*pbf)->next);
	}
	*pbf = calloc(1, sizeof **pbf);
	strcpy((*pbf)->name, name);
	return *pbf;
}

void compile_bits(struct regs *r, char *s)
{
	struct regs *fr = r;
	struct bitfield *b;
	char *t, sym[MAXLEN], regname[MAXLEN], bitname[MAXLEN], tmp[MAXLEN];
	uint64_t v, no_bits = 0;

	while (*s) {
		if (!define(&s)) {
			// we hit a define 
			symbol(&s, sym);
			v = number(&s);

			// clear so NUL is set
			memset(regname, 0, sizeof regname);
			memset(bitname, 0, sizeof bitname);
			memset(tmp, 0, sizeof tmp);

			// split it up
			t = strstr(sym, "__");
			if (t) {
				memcpy(regname, sym, t - sym);
				fr = find_reg(r, fr, regname);
				if (fr) {
					strcpy(tmp, t + 2);              // points to bitname__SHIFT or bitname_MASK
					t = strstr(tmp, "__SHIFT");
					if (t) {
						memcpy(bitname, tmp, t - tmp);
						b = find_bit(fr, bitname);
						if (!b)
							b = add_bit(fr, bitname);
						b->shift = v;
					} else {
						memcpy(bitname, tmp, strlen(tmp) - 5);
						b = find_bit(fr, bitname);
						if (!b)
							b = add_bit(fr, bitname);
						b->mask = v;
					}
					fprintf(stderr, "%8"PRIu64"   bits added...\r", ++no_bits >> 1);
				} else {
					fprintf(stderr, "[WARNING]: Register '%s' not found in offset database\n", regname);
				}
			}
		}
	}
}

struct regs *compile_register(char *s)
{
	struct regs *r, *or;
	char sym[MAXLEN];
	uint64_t v, no_reg = 0;
	int s15;

	s15 = getenv("UMR_NO_SOC15") ? 0 : 1;

	or = r = calloc(1, sizeof *r);
	r->idx = r->addr = 0xFFFFFFFF;
	while (*s) {
		if (!define(&s)) {
			// we hit a define
			symbol(&s, sym);
			v = number(&s);

			// is it a register? (TODO: other types ...)
			r->type = REG_ERROR;
			if (!memcmp(sym, "mm", 2) || !memcmp(sym, "reg", 3))
				r->type = REG_MMIO;
			else if (!memcmp(sym, "ix", 2))
				r->type = REG_SMC;
			else if (!memcmp(sym, "cfg", 3))
				r->type = REG_PCIE;
			else if (!memcmp(sym, "smn", 3))
				r->type = REG_SMN;

			if (r->type != REG_ERROR) {
				// is this an IDX define?
				if (strstr(sym, "_BASE_IDX")) {
					r->idx = v;
				} else {
					r->addr = v;
					strcpy(r->name, sym);
				}

				// if we have both we're done
				if ((!s15 || r->type != REG_MMIO || r->idx != 0xFFFFFFFF) && r->addr != 0xFFFFFFFF) {
					r->next = calloc(1, sizeof *r);
					r = r->next;
					r->idx = r->addr = 0xFFFFFFFF;
					fprintf(stderr, "%8"PRIu64"   registers added...\r", ++no_reg);
				}
			}
		}
	}
	return or;
}

void bits_start_stop(uint64_t mask, int *start, int *stop)
{
	if (!mask) {
		*start = *stop = 0;
		return;
	}
	*start = 0;
	while (!(mask & 1)) {
		mask >>= 1;
		++*start;
	}
	*stop = *start - 1;
	while (mask) {
		mask >>= 1;
		++*stop;
	}
}

uint32_t update_regs(struct regs *r)
{
	struct bitfield *bits;
	uint64_t max_bit, no_bits;
	uint32_t no_regs = 0;
	while (r) {
		++no_regs;
		max_bit = no_bits = 0;
		bits = r->bits;
		while (bits) {
			++no_bits;
			bits_start_stop(bits->mask, &bits->start, &bits->stop);
			if (bits->mask == 0)
				fprintf(stderr, "[WARNING]: %s.%s has no mask\n", r->name, bits->name);
			if (bits->stop > max_bit)
				max_bit = bits->stop;
			bits = bits->next;
		}
		r->nobits = no_bits;
		if (max_bit > 32)
			r->is64 = 1;
		r = r->next;
	}
	return no_regs - 1; // subtract one because the last entry is unused
}

struct soc15 {
	char name[MAXLEN];
	uint64_t off[32][8]; // inst, seg
	struct soc15 *next;
};

struct soc15 *find_soc(struct soc15 **soc, char *ipname)
{
	if (!*soc) {
		*soc = calloc(1, sizeof **soc);
		strcpy((*soc)->name, ipname);
		return *soc;
	} else {
		struct soc15 *s = *soc;
		while (s) {
			if (!strcmp(s->name, ipname))
				return s;
			if (!s->next) {
				s->next = calloc(1, sizeof *s);
				s = s->next;
				strcpy(s->name, ipname);
				return s;
			} else {
				s = s->next;
			}
		}
		return NULL;
	}
}

struct soc15 *compile_soc15(char *s)
{
	struct soc15 *soc, *psoc;
	char sym[MAXLEN], ipname[MAXLEN];
	uint64_t v;
	int inst, seg;
	char *sp;

	soc = psoc = NULL;

	while (*s) {
		if (!define(&s)) {
			// we hit a define
			symbol(&s, sym);
			sp = strstr(sym, "_BASE__INST");
			memset(ipname, 0, sizeof ipname);
			if (sp) {
				v = number(&s);
				memcpy(ipname, sym, sp - sym);
				psoc = find_soc(&soc, ipname);
				if (sscanf(sp + 7, "INST%d_SEG%d", &inst, &seg) == 2)
					psoc->off[inst][seg] = v;
				else
					fprintf(stderr, "[WARNING]: Invalid define [%s]\n", sp + 7);
			}
		}
	}
	return soc;
}

int istr_cmp(const char* a, const char* b)
{
	unsigned char a_up;
	unsigned char b_up;

	for(;;) {
		a_up = (unsigned char)toupper(*a);
		b_up = (unsigned char)toupper(*b);

		if (a_up != b_up || !a_up) {
			break;
		}
		a++;
		b++;
	}
    return a_up - b_up;
}

static int reg_sort(const void *a, const void *b)
{
	const struct regs **A = a, **B = b;
	return istr_cmp((*A)->name, (*B)->name);
}

int main(int argc, char **argv)
{
	char *rf, *bf;
	FILE *f;
	uint64_t size;
	uint32_t no_regs;
	struct regs *r, **sr;
	struct bitfield *b;
	struct soc15 *s;
	int x, y;

	if (argc != 3 && argc != 2) {
		fprintf(stderr, "Usage:\n"
"\tTo compile a register file:\n\t\t%s offset_header sh_mask_header\n\n"
"\tTo compile a SOC15 ASIC offset file:\n\t\t%s ipoffset_header\n\n\n"
"\tBoth commands output the compiled output to 'stdout' and messages/errors to 'stderr'.\n", argv[0], argv[0]);
		return EXIT_FAILURE;
	}

	if (argc == 3) {
		f = fopen(argv[1], "rb");
		if (!f) {
			fprintf(stderr, "[ERROR]: Could not open file '%s'\n", argv[1]);
			return EXIT_FAILURE;
		}
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
		rf = calloc(1, size + 1);
		fread(rf, 1, size, f);
		fclose(f);

		f = fopen(argv[2], "rb");
		if (!f) {
			fprintf(stderr, "[ERROR]: Could not open file '%s'\n", argv[2]);
			return EXIT_FAILURE;
		}
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
		bf = calloc(1, size + 1);
		fread(bf, 1, size, f);
		fclose(f);

		fprintf(stderr, "Compiling registers...\n");
		r = compile_register(rf);
		fprintf(stderr, "\nCompiling bits...                                                     \n");
		compile_bits(r, bf);
		fprintf(stderr, "\nUpdating registers...                                                 \n");
		no_regs = update_regs(r);
		printf("%"PRIu32"\n", no_regs);
		fprintf(stderr, "Done...\n");

		sr = calloc(sizeof *sr, no_regs);
		x = 0;
		while (r && r->addr != 0xFFFFFFFF) {
			sr[x++] = r;
			r = r->next;
		}
		qsort(sr, no_regs, sizeof sr[0], reg_sort);
		for (x = 0; x < no_regs; x++) {
			r = sr[x];
			printf("%s %d 0x%"PRIx64" %"PRIu32" %"PRIu32" %"PRIu32"\n", r->name, r->type, r->addr, r->nobits, r->is64, r->idx);
			b = r->bits;
			while (b) {
				printf("\t%s %d %d\n", b->name, b->start, b->stop);
				b = b->next;
			}
		}
	} else {
		f = fopen(argv[1], "rb");
		if (!f) {
			fprintf(stderr, "[ERROR]: Could not open file '%s'\n", argv[1]);
			return EXIT_FAILURE;
		}
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
		rf = calloc(1, size + 1);
		fread(rf, 1, size, f);
		fclose(f);
		s = compile_soc15(rf);
		while (s) {
			printf("%s\n", s->name);
			for (x = 0; x < 32; x++) {
				printf("\t");
				for (y = 0; y < 8; y++) {
					printf("0x%08"PRIx64" ", s->off[x][y]);
				}
				printf("\n");
			}
			s = s->next;
		}
	}
	return 0;
}

