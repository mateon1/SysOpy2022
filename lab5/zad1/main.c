// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

typedef struct hashcell {
	char *key;
	void *val;
} _hashcell;

typedef struct hashmap {
	_hashcell *table;
	uint64_t size; // size always a power of two
	uint64_t elems;
	uint64_t tombs;
	void (*elem_free)(void*);
} hashmap;

static uint64_t hashfn(char *str) { // Decent mixing, not cryptographically safe, not safe to adversarial attack
	uint64_t ret = 0xB5F3EC974D101806;
	while (*str) {
		uint64_t part = 0x5555555555555555;
		for (int i = 0; i < 8 && *str; str++)
			part ^= ((unsigned)*str << (8 * i));
		ret ^= part;
		ret ^= (ret << 13);
		ret ^= (ret >> 7);
		ret ^= (ret << 17);
	}
	ret ^= (ret << 13);
	ret ^= (ret >> 7);
	ret ^= (ret << 17);
	ret ^= (ret << 13);
	ret ^= (ret >> 7);
	ret ^= (ret << 17);
	return ret;
}

static _hashcell *_hash_get_cell(_hashcell *tab, uint64_t size, char *key) {
	assert(size > 0);
	assert((size & (size-1)) == 0); // power of two
	uint64_t hash = hashfn(key);
	uint64_t mask = size - 1;
	uint64_t inc = ((hash >> 5) - hash) | 1;
	uint64_t idx = hash;
	_hashcell *res;
	while ((res = &tab[idx & mask]) && res->key != NULL) {
		if (strcmp(key, res->key) == 0) {
			return res;
		}
		idx += inc; // bad for cache locality, but meh
	}
	return res; // empty slot
}

static void _hash_resize_if_needed(hashmap *m) {
	// if less than 80% occupied, it's fine
	if (m->size * 4 > (m->elems + m->tombs) * 5) {
		// TODO: Overflow!!! (won't happen in practice unless in 128-bit address space or something equally crazy)
		return;
	}
	uint64_t newsize = 4;
	while (newsize << 1 && newsize/3 <= m->elems) {
		newsize <<= 1;
	}
	_hashcell *newtab = calloc(newsize, sizeof(_hashcell));
	if (newtab == NULL) {
		printf("Hash table allocation failed!\n");
		exit(65);
	}
	for (uint64_t i = 0; i < m->size; i++) {
		_hashcell *res = &m->table[i];
		if (res->val) {
			_hashcell *new = _hash_get_cell(newtab, newsize, res->key);
			new->val = res->val;
			new->key = res->key;
		} else if (res->key) { // don't copy tombstones
			free(res->key);
		}
	}
	free(m->table);
	m->table = newtab;
	m->size = newsize;
	m->tombs = 0;
}

static void hash_add(hashmap *m, char *key, void *val) {
	assert(val);
	if (m->size == 0) _hash_resize_if_needed(m);
	_hashcell *res = _hash_get_cell(m->table, m->size, key);
	if (res->key != NULL) {
		// old value, just rewrite the value
		if (res->val == NULL) {
			// was tombstone
			m->elems++; m->tombs--;
		} else if (m->elem_free) m->elem_free(res->val);
		res->val = val;
	} else {
		assert(res->val == NULL);
		res->val = val;
		res->key = malloc(strlen(key)+1);
		if (res->key == NULL) {
			printf("Hash table key allocation failed\n");
			exit(65);
		}
		strcpy(res->key, key);
		m->elems++;
		_hash_resize_if_needed(m);
	}
}

//static void hash_del(hashmap *m, char *key) {
//	_hashcell *res = _hash_get_cell(m->table, m->size, key);
//	if (res->key != NULL && res->val != NULL) {
//		if (m->elem_free) m->elem_free(res->val);
//		res->val = NULL;
//		m->elems--;
//		m->tombs++;
//	}
//}

static void* hash_get(hashmap *m, char *key) {
	_hashcell *res = _hash_get_cell(m->table, m->size, key);
	return res->val;
}

// Dynamic vector with automatic resize
static size_t next_size(size_t x) {
	if (x < 4) return 4; // optimization, don't do tiny allocations
	x <<= 1;
	x--;
	while ((x & (x - 1)) != 0) { // while isn't a power of two
		x &= (x - 1); // clear lowest bit
	}
	return x;
}

typedef struct vec {
	size_t cap;
	size_t len;
	void* *data;
} vec_t;

//static void vec_alloc(vec_t *vec, size_t len) {
//	vec->cap = len;
//	vec->len = 0;
//	vec->data = malloc(len * sizeof(void*));
//	if (vec->data == NULL) {
//		fprintf(stderr, "Vec allocation failed\n");
//		exit(65);
//	}
//}

static void vec_realloc(vec_t *vec, size_t newlen) {
	assert(newlen > vec->cap);
	void* *olddata = vec->data;
	vec->data = malloc(newlen * sizeof(void*));
	if (vec->len) {
		memcpy(vec->data, olddata, vec->len * sizeof(void*));
		vec->cap = newlen;
	}
	free(olddata);
}

static void vec_free(vec_t *vec, bool free_elems) {
	assert(vec->data != NULL);
	if (free_elems) {
		for (size_t i = 0; i < vec->len; i++) free(vec->data[i]);
	}
	free(vec->data);
	vec->data = NULL;
	vec->len = 0;
	vec->cap = 0;
}

static void vec_ensure(vec_t *vec, size_t len) {
	if (len > vec->cap) {
		size_t wanted = next_size(len);
		assert(wanted >= len);
		vec_realloc(vec, wanted);
		if (vec->data == NULL) {
			fprintf(stderr, "Vec allocation failed\n");
			exit(65);
		}
	}
}

static void vec_push(vec_t *vec, void* elem) {
	vec_ensure(vec, vec->len + 1);
	vec->data[vec->len++] = elem;
}

//static void* vec_pop(vec_t *vec) {
//	if (!vec->len) return NULL;
//	return vec->data[--vec->len];
//}

// Sized string type. ->cap and ->len do not count terminating NUL byte.
struct sizedstr {
	size_t cap;
	size_t len;
	char *data;
};

static void ss_alloc(struct sizedstr *ss, size_t len) {
	ss->cap = len;
	ss->len = 0;
	ss->data = calloc(len + 1, 1);
}

static void ss_realloc(struct sizedstr *ss, size_t newlen) {
	assert(newlen > ss->cap);
	char *olddata = ss->data;
	ss->data = calloc(newlen + 1, 1);
	if (ss->data != NULL && olddata != NULL) {
		strcpy(ss->data, olddata);
		ss->cap = newlen;
	}
	free(olddata);
}

static void ss_free(struct sizedstr *ss) {
	assert(ss->data != NULL);
	free(ss->data);
	ss->data = NULL;
	ss->len = 0;
	ss->cap = 0;
}

static void ss_ensure(struct sizedstr *ss, size_t len, bool realloc) {
	if (len > ss->cap) {
		size_t wanted = next_size(len);
		assert(wanted >= len);
		if (realloc) {
			ss_realloc(ss, wanted);
		} else {
			if (ss->data != NULL) ss_free(ss);
			ss_alloc(ss, wanted);
		}
		if (ss->data == NULL) {
			fprintf(stderr, "String allocation failed\n");
			exit(65);
		}
	}
}

//static void ss_assign(struct sizedstr *ss, const char* s) {
//	size_t newlen = strlen(s);
//	ss_ensure(ss, newlen, false);
//	strcpy(ss->data, s);
//	ss->len = newlen;
//}

static void ss_push(struct sizedstr *ss, const char* s, size_t len) {
	ss_ensure(ss, ss->len + len, true);
	memcpy(ss->data + ss->len, s, len);
	ss->len += len;
	ss->data[ss->len] = 0;
}

static char* ss_take(struct sizedstr *ss) {
	char *ret = ss->data;
	ss->len = 0;
	ss->cap = 0;
	ss->data = NULL;
	return ret;
}

/// ACTUAL PROGRAM CODE BELOW

/*
Grammar sketch:

NAME = /[^\x00-\x20|]([^\x00-\x1f|]*[^\x00-\x20|])?/  -- can contain whitespace, but not leading or trailing. Otherwise whitespace is ignored
IDENT = /[^\x00-\x20|=]([^\x00-\x1f|=]*[^\x00-\x20|=])?/  -- same as above, but without equal signs

definition = IDENT '=' defexpression
defexpression = NAME { '|' NAME }
expression = IDENT { '|' IDENT }

line = definition | expression

grammar = line { '\n' line }

NOTE: This grammar is flawed as a batch file syntax, does not allow inputting pipes inside a command easily, must use bash constructs such as $'\x7c' to do so.
*/

struct sm_data {
	uint64_t lineno; // for error reporting

	hashmap definitions; // char* -> vec_t (of char*)

	vec_t cur_expr; // current vec of names
	struct sizedstr scratch; // scratch space for not fully parsed name
};

#define M_INSIDE 1
#define M_DEF 2
#define M_EXP 4
#define IS_INSIDE(s) (((s)&M_INSIDE) != 0)
#define IS_DEF(s) (((s)&M_DEF) != 0)
#define IS_EXP(s) (((s)&M_EXP) != 0)

enum sm_state {
	S_INVALID = -1,
	S_DEF_BEFORE_WORD = M_DEF,
	S_DEF_INSIDE_WORD = M_DEF | M_INSIDE,
	S_EXP_BEFORE_WORD = M_EXP,
	S_EXP_INSIDE_WORD = M_EXP | M_INSIDE,
	S_ANY_BEFORE_WORD = M_DEF | M_EXP,
	S_ANY_INSIDE_WORD = M_DEF | M_EXP | M_INSIDE,
};

static void hash_free_fn(void *vec) {
	vec_t *v = (vec_t*)vec;
	vec_free(v, true);
	free(v);
}

static void parser_init(struct sm_data *ctx, enum sm_state *state) {
	memset(ctx, 0, sizeof(struct sm_data));
	ctx->lineno = 1;
	ctx->definitions.elem_free = hash_free_fn;
	*state = S_ANY_BEFORE_WORD;
}

static void parser_sm(struct sm_data *ctx, enum sm_state *s, char *begin, char *end) {
	char *nl = memchr(begin, '\n', end - begin);
	char *eq = memchr(begin, '=', end - begin);
	char *pi = memchr(begin, '|', end - begin);
loop:
	while (begin < end && *s != S_INVALID) {
		if (!IS_INSIDE(*s)) {
			while (begin < end && *begin == ' ')
				begin++;
			if (begin == end) return;
			switch (*begin) {
				case '=': case '|':
					*s = S_INVALID;
					fprintf(stderr, "Syntax error on line %ld, expected word, got '%c'\n", ctx->lineno, *begin);
					return;
				case '\n':
					if (*s != S_ANY_BEFORE_WORD) {
						*s = S_INVALID;
						fprintf(stderr, "Syntax error on line %ld, expected word, got newline\n", ctx->lineno);
						return;
					}
					// empty line
					begin++; ctx->lineno++;
					nl = memchr(begin, '\n', end - begin);
					goto loop;
				default:
					if (*begin < 0x20) {
						*s = S_INVALID;
						fprintf(stderr, "Syntax error on line %ld, expected word, got control character\n", ctx->lineno);
						return;
					}
					*s |= M_INSIDE;
			}
		}
		// inside non-empty word
		char *term = nl;
		// don't treat '=' specially when in S_DEF_* expressions, so commands can have = in them
		if (((*s) & (M_DEF | M_EXP)) != M_DEF && eq && (!term || term > eq)) term = eq;
		if (pi && (!term || term > pi)) term = pi;
		if (!term) {
			// No word terminator in sight, consume everything
			ss_push(&ctx->scratch, begin, end - begin);
			return;
		}
		// otherwise, the current word will end at the terminator; eat everything until then
		ss_push(&ctx->scratch, begin, term - begin); // zero-byte pushes are fine
		begin = term;

		// strip whitespace from scratch, then push to expression vector
		for (char *p = &ctx->scratch.data[ctx->scratch.len-1]; *p == ' '; *p-- = 0, ctx->scratch.len--);

		vec_push(&ctx->cur_expr, (void*)ss_take(&ctx->scratch));
		*s &= ~M_INSIDE;

		switch (*begin++) {
			case '\n': // always valid - perform operation, if DEF add definition from vec, otherwise run pipeline
				if (IS_EXP(*s)) {
					// Fail early for expressions referencing nonexistent definitions
					char *name = (char*)ctx->cur_expr.data[ctx->cur_expr.len - 1];
					void *res = hash_get(&ctx->definitions, name);
					if (res == NULL) {
						*s = S_INVALID;
						fprintf(stderr, "Error on line %ld, unknown definition \"%s\"\n", ctx->lineno, name);
						return;
					}

					//printf("Running pipeline:\n");
					size_t nchild = 0;
					int pipefd[2] = {0, 0};
					for (uint64_t i = 0; i < ctx->cur_expr.len; i++) {
						vec_t *elem = (vec_t*)hash_get(&ctx->definitions, ctx->cur_expr.data[i]);
						assert(elem != NULL); // ensured by early fail cases
						for (uint64_t j = 1; j < elem->len; j++) {
							//printf("subcommand: %s, pipefd %d %d\n", elem->data[j], pipefd[0], pipefd[1]);
							nchild++;
							bool first = j == 1 && i == 0;
							bool last = j == elem->len - 1 && i == ctx->cur_expr.len - 1;
							int lastinfd = pipefd[0];
							if (!last) {
								if (pipe(pipefd) < 0) {
									perror("Could not create pipe");
									exit(1);
								}
							}
							int cpid = fork();
							if (cpid < 0) {
								perror("fork() failed");
								exit(1);
							}
							if (cpid == 0) {
								// child, setup pipes and execute
								if (!first) {
									if (dup2(lastinfd, STDIN_FILENO) < 0) {
										perror("dup2() failed");
										exit(1);
									}
									if (close(lastinfd) < 0) {
										perror("close failed");
										exit(1);
									}
								}
								if (!last) {
									if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
										perror("dup2() failed");
										exit(1);
									}
									if (close(pipefd[0]) < 0) {
										perror("close failed");
										exit(1);
									}
									if (close(pipefd[1]) < 0) {
										perror("close failed");
										exit(1);
									}
								}
								
								execlp("sh", "sh", "-c", elem->data[j], NULL);
								_exit(-1); // unreachable
							} else {
								// parent does not need pipe fds
								if (!last) {
									if (close(pipefd[1]) < 0) {
										perror("close failed");
										exit(1);
									}
								}
								if (!first) {
									if (close(lastinfd) < 0) {
										perror("close failed");
										exit(1);
									}
								}
							}
						}
					}
					vec_free(&ctx->cur_expr, true);
					memset(&ctx->cur_expr, 0, sizeof(vec_t));
					bool errored = false;
					for (size_t i = 0; i < nchild; i++) {
						int wstatus;
						if (wait(&wstatus) < 0) {
							errored = true;
							perror("wait() failed");
							continue;
						}
					}
					if (errored) exit(1);
				} else {
					// Define, key is first vector element, rest are commands
					char *key = (char*)ctx->cur_expr.data[0];
					vec_t *val = malloc(sizeof(vec_t));
					memcpy(val, &ctx->cur_expr, sizeof(vec_t));
					memset(&ctx->cur_expr, 0, sizeof(vec_t));
					hash_add(&ctx->definitions, key, (void*)val);
				}
				ctx->lineno++;
				nl = memchr(begin, '\n', end - begin);
				eq = memchr(begin, '=', end - begin); // we may have broken eq consistency if there were '=' in DEF expressions
				*s = S_ANY_BEFORE_WORD;
				break;
			case '|': // always valid, transition ANY -> EXP
				if (IS_DEF(*s) && IS_EXP(*s)) *s = S_EXP_BEFORE_WORD;
	
				if (IS_EXP(*s)) {
					// Fail early for expressions referencing nonexistent definitions
					char *name = (char*)ctx->cur_expr.data[ctx->cur_expr.len - 1];
					void *res = hash_get(&ctx->definitions, name);
					if (res == NULL) {
						*s = S_INVALID;
						fprintf(stderr, "Error on line %ld, unknown definition \"%s\"\n", ctx->lineno, name);
						return;
					}
				}

				pi = memchr(begin, '|', end - begin);
				break;
			case '=': // only valid if ANY, transition -> DEF
				if (*s != S_ANY_BEFORE_WORD) {
					assert(*s == S_EXP_BEFORE_WORD);
					*s = S_INVALID;
					fprintf(stderr, "Syntax error on line %ld, expected word or pipe, got equals sign\n", ctx->lineno);
					return;
				}
				*s = S_DEF_BEFORE_WORD;

				eq = memchr(begin, '=', end - begin);
				break;
			default:
				assert(false && "Unreachable default case");
				_exit(3);
		}
	}
}

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("Usage: %s <input file>\n", argc > 0 ? argv[0] : "main");
		exit(2);
	}

	struct sm_data ctx;
	enum sm_state state;

	parser_init(&ctx, &state);

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("Failed to open file");
		exit(1);
	}

	char buf[4096];
	ssize_t nread;

	while ((nread = read(fd, buf, sizeof(buf))) > 0) {
		parser_sm(&ctx, &state, buf, buf+nread);
		if (state == S_INVALID) {
			exit(1);
		}
	}

	if (nread < 0) {
		perror("Failed to read file");
		exit(1);
	}

	return 0;
}
