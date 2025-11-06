#ifndef DX_REGEX_INTERNAL_H
#define DX_REGEX_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "regex.h"

typedef enum {
  R_NULL,   // ∅ (matches nothing)
  R_EPS,    // ε (empty string)
  R_CHAR,   // single literal character
  R_CLASS,  // character class [abc...] (no ranges)
  R_ALT,    // alternation A|B
  R_CONCAT, // concatenation AB
  R_STAR    // Kleene star A*
} RKind;

struct dx_regex {
  RKind kind;
  union {
    unsigned char ch; // for R_CHAR
    struct {          // for R_CLASS
      bool set[256];
      int count; // number of characters in set
    } cls;
    struct { // for R_ALT, R_CONCAT
      struct dx_regex *a;
      struct dx_regex *b;
    } pair;
    struct dx_regex *sub; // for R_STAR
  } u;
};

typedef struct {
  const char *s;
  size_t i;
  char *err;
} Parser;

#ifdef __cplusplus
extern "C" {
#endif

dx_regex *mk_null(void);
dx_regex *mk_eps(void);
dx_regex *mk_char(unsigned char c);
dx_regex *mk_class_from_set(const bool set[256], int count);
dx_regex *mk_alt(dx_regex *a, dx_regex *b);
dx_regex *mk_concat(dx_regex *a, dx_regex *b);
dx_regex *mk_star(dx_regex *sub);

dx_regex *smart_alt(dx_regex *a, dx_regex *b);
dx_regex *smart_concat(dx_regex *a, dx_regex *b);
dx_regex *smart_star(dx_regex *a);

void dx_free_internal(dx_regex *r);
dx_regex *clone_regex(const dx_regex *r);
int is_nullable(const dx_regex *r);
dx_regex *derive(const dx_regex *r, unsigned char c);

char p_peek(Parser *p);
char p_get(Parser *p);
bool p_eof(Parser *p);
void p_set_error(Parser *p, const char *fmt, ...);

bool is_atom_start(char c);
dx_regex *parse_class(Parser *p);
dx_regex *parse_atom(Parser *p);
dx_regex *parse_unary(Parser *p);
dx_regex *parse_concat(Parser *p);
dx_regex *parse_alt(Parser *p);

#ifdef __cplusplus
}
#endif

#endif
