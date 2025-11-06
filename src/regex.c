#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Static singletons for R_NULL and R_EPS to simplify memory management
static struct dx_regex DX_NULL_NODE = {.kind = R_NULL};
static struct dx_regex DX_EPS_NODE = {.kind = R_EPS};

// function to make null and eps nodes
static inline dx_regex *mk_null(void) { return &DX_NULL_NODE; }
static inline dx_regex *mk_eps(void) { return &DX_EPS_NODE; }

// function to make single char node
static dx_regex *mk_char(unsigned char c) {
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r)
    return mk_null();
  r->kind = R_CHAR;
  r->u.ch = c;
  return r;
}

// function to make a class [abc]
static dx_regex *mk_class_from_set(const bool set[256], int count) {
  if (count <= 0) {
    return mk_null();
  }
  if (count == 1) {
    for (int i = 0; i < 256; i++) {
      if (set[i])
        return mk_char((unsigned char)i);
    }
  }
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r)
    return mk_null();
  r->kind = R_CLASS;
  memcpy(r->u.cls.set, set, 256 * sizeof(bool));
  r->u.cls.count = count;
  return r;
}

// function for making node a|b
static dx_regex *mk_alt(dx_regex *a, dx_regex *b) {
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r) {
    return mk_null();
  }
  r->kind = R_ALT;
  r->u.pair.a = a;
  r->u.pair.b = b;
  return r;
}

// function for making node ab
static dx_regex *mk_concat(dx_regex *a, dx_regex *b) {
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r) {
    return mk_null();
  }
  r->kind = R_CONCAT;
  r->u.pair.a = a;
  r->u.pair.b = b;
  return r;
}

// function for making node a*, (ab)*, etc
static dx_regex *mk_star(dx_regex *sub) {
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r) {
    return mk_null();
  }
  r->kind = R_STAR;
  r->u.sub = sub;
  return r;
}

// Forward declarations
static void dx_free_internal(dx_regex *r);
static dx_regex *clone_regex(const dx_regex *r);
static int is_nullable(const dx_regex *r);
static dx_regex *derive(const dx_regex *r, unsigned char c);

static dx_regex *smart_alt(dx_regex *a, dx_regex *b) {
  if (a->kind == R_NULL) { // ∅ | B => B
    return b;
  }
  if (b->kind == R_NULL) { // A | ∅ => A
    return a;
  }
  // If they point to the same node, free mem and return other
  if (a == b) {
    dx_free_internal(b);
    return a;
  }
  return mk_alt(a, b);
}

static dx_regex *smart_concat(dx_regex *a, dx_regex *b) {
  if (a->kind == R_NULL || b->kind == R_NULL) {
    dx_free_internal(a);
    dx_free_internal(b);
    return mk_null(); // ∅
  }
  if (a->kind == R_EPS) {
    dx_free_internal(a);
    return b; // ε·B => B
  }
  if (b->kind == R_EPS) {
    dx_free_internal(b);
    return a; // A·ε => A
  }
  return mk_concat(a, b);
}

static dx_regex *smart_star(dx_regex *a) {
  if (a->kind == R_NULL || a->kind == R_EPS) {
    dx_free_internal(a);
    return mk_eps(); // ∅* = ε, ε* = ε
  }
  if (a->kind == R_STAR) {
    // (A*)* = A*
    return a;
  }
  return mk_star(a);
}

static void dx_free_internal(dx_regex *r) {
  if (!r)
    return;
  if (r == &DX_NULL_NODE || r == &DX_EPS_NODE)
    return; // singletons
  switch (r->kind) {
  case R_NULL:
  case R_EPS:
  case R_CHAR:
  case R_CLASS:
    free(r);
    return;
  case R_ALT:
  case R_CONCAT:
    dx_free_internal(r->u.pair.a);
    dx_free_internal(r->u.pair.b);
    free(r);
    return;
  case R_STAR:
    dx_free_internal(r->u.sub);
    free(r);
    return;
  }
}

void dx_free(dx_regex *r) { dx_free_internal(r); }

static dx_regex *clone_regex(const dx_regex *r) {
  if (!r)
    return NULL;
  if (r == &DX_NULL_NODE)
    return mk_null();
  if (r == &DX_EPS_NODE)
    return mk_eps();
  switch (r->kind) {
  case R_NULL:
    return mk_null();
  case R_EPS:
    return mk_eps();
  case R_CHAR:
    return mk_char(r->u.ch);
  case R_CLASS: {
    return mk_class_from_set(r->u.cls.set, r->u.cls.count);
  }
  case R_ALT: {
    dx_regex *a = clone_regex(r->u.pair.a);
    dx_regex *b = clone_regex(r->u.pair.b);
    return mk_alt(a, b);
  }
  case R_CONCAT: {
    dx_regex *a = clone_regex(r->u.pair.a);
    dx_regex *b = clone_regex(r->u.pair.b);
    return mk_concat(a, b);
  }
  case R_STAR: {
    dx_regex *s = clone_regex(r->u.sub);
    return mk_star(s);
  }
  }
  return mk_null();
}

// check nullibility
// - ε and A* are nullable
// - A|B is nullable if either A or B is nullable
// - AB is nullable if both A and B are nullable
// - A single literal or class is not nullable
// - ∅ is not nullable
static int is_nullable(const dx_regex *r) {
  switch (r->kind) {
  case R_NULL:
    return 0;
  case R_EPS:
    return 1;
  case R_CHAR:
    return 0;
  case R_CLASS:
    return 0;
  case R_ALT:
    return is_nullable(r->u.pair.a) || is_nullable(r->u.pair.b);
  case R_CONCAT:
    return is_nullable(r->u.pair.a) && is_nullable(r->u.pair.b);
  case R_STAR:
    return 1;
  }
  return 0;
}

static dx_regex *derive(const dx_regex *r, unsigned char c) {
  switch (r->kind) {
    // d(∅, c) = ∅, d(ε, c) = ∅
  case R_NULL:
  case R_EPS:
    return mk_null();
    // d(a, c) = ε if a == c, else ∅
  case R_CHAR:
    return (r->u.ch == c) ? mk_eps() : mk_null();
    // d([S], c) = ε if c ∈ S
  case R_CLASS:
    return r->u.cls.set[c] ? mk_eps() : mk_null();
  case R_ALT: {
    // d(A | B, c) = d(A, c) | d(B, c)
    dx_regex *da = derive(r->u.pair.a, c);
    dx_regex *db = derive(r->u.pair.b, c);
    return smart_alt(da, db);
  }
  case R_CONCAT: {
    // d(AB) = d(A)B | (nullable(A) ? d(B) : ∅)
    dx_regex *dA = derive(r->u.pair.a, c);
    dx_regex *Bclone = clone_regex(r->u.pair.b); // ensure no sharing
    dx_regex *term1 = smart_concat(dA, Bclone);

    if (is_nullable(r->u.pair.a)) {
      dx_regex *dB = derive(r->u.pair.b, c);
      return smart_alt(term1, dB);
    } else
      return term1;
  }
  case R_STAR: {
    // d(A*) = d(A) A*
    dx_regex *dA = derive(r->u.sub, c);
    dx_regex *Astar = smart_star(clone_regex(r->u.sub));
    return smart_concat(dA, Astar);
  }
  }
  return mk_null();
}

typedef struct {
  const char *s;
  size_t i;
  char *err; // malloc'd error string, or NULL
} Parser;

static char p_peek(Parser *p) { return p->s[p->i]; }

static char p_get(Parser *p) {
  char c = p->s[p->i];
  if (c != '\0')
    p->i++;
  return c;
}

static bool p_eof(Parser *p) { return p->s[p->i] == '\0'; }

static void p_set_error(Parser *p, const char *fmt, ...) {
  if (p->err)
    return;
  // Create message with context
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  char ctx[64];
  size_t len = strlen(p->s);
  size_t start = p->i > 10 ? p->i - 10 : 0;
  size_t end = (p->i + 10 < len) ? p->i + 10 : len;
  size_t ctx_len = end - start;
  if (ctx_len > sizeof(ctx) - 1)
    ctx_len = sizeof(ctx) - 1;
  memcpy(ctx, p->s + start, ctx_len);
  ctx[ctx_len] = '\0';

  char final[512];
  snprintf(final, sizeof(final), "Parse error at pos %zu: %s\nContext: \"%s\"",
           p->i, buf, ctx);

  p->err = (char *)malloc(strlen(final) + 1);
  if (p->err)
    strcpy(p->err, final);
}

// Forward declarations of parser functions
static dx_regex *parse_alt(Parser *p);
static dx_regex *parse_concat(Parser *p);
static dx_regex *parse_unary(Parser *p);
static dx_regex *parse_atom(Parser *p);

static bool is_atom_start(char c) {
  if (c == '\0')
    return false;
  if (c == '|' || c == ')' || c == '*')
    return false;
  return true; // includes '(' '[' '\' and any literal
}

// parse [abc] type
static dx_regex *parse_class(Parser *p) {
  // assumes '[' already consumed
  bool set[256] = {false};
  int count = 0;
  bool closed = false;

  while (!p_eof(p)) {
    char c = p_get(p);
    if (c == ']') {
      closed = true;
      break;
    }
    if (c == '\\') {
      if (p_eof(p)) {
        p_set_error(p, "unterminated escape in character class");
        break;
      }
      c = p_get(p);
    }
    unsigned char uc = (unsigned char)c;
    if (!set[uc]) {
      set[uc] = true;
      count++;
    }
  }
  if (!closed && !p->err)
    p_set_error(p, "unterminated character class (missing ']')");

  return mk_class_from_set(set, count);
}

static dx_regex *parse_atom(Parser *p) {
  char c = p_peek(p);
  // parse (A|B) type
  if (c == '(') {
    p_get(p); // consume '('
    dx_regex *inside = parse_alt(p);
    if (p_peek(p) == ')') {
      p_get(p);
      return inside;
    } else {
      if (!p->err)
        p_set_error(p, "expected ')'");
      dx_free_internal(inside);
      return mk_null();
    }
  } // parse [abc] type
  else if (c == '[') {
    p_get(p); // consume '['
    return parse_class(p);
  } else if (c == '\\') {
    p_get(p); // consume '\'
    if (p_eof(p)) {
      p_set_error(p, "dangling escape at end of pattern");
      return mk_null();
    }
    char lit = p_get(p);
    return mk_char((unsigned char)lit);
  } else if (is_atom_start(c)) {
    p_get(p);
    return mk_char((unsigned char)c);
  } else {
    p_set_error(p, "expected atom");
    return mk_null();
  }
}

static dx_regex *parse_unary(Parser *p) {
  dx_regex *a = parse_atom(p);
  if (p->err)
    return a;
  while (p_peek(p) == '*') {
    p_get(p); // consume '*'
    a = smart_star(a);
  }
  return a;
}

static dx_regex *parse_concat(Parser *p) {
  dx_regex *left = NULL;
  while (is_atom_start(p_peek(p))) {
    dx_regex *u = parse_unary(p);
    if (p->err) {
      dx_free_internal(left);
      return u;
    }
    // check for first itr of loop
    if (!left) {
      left = u;
    } else {
      left = smart_concat(left, u);
    }
  }
  // Empty concatenation is ε
  if (!left) {
    left = mk_eps();
  }
  return left;
}

static dx_regex *parse_alt(Parser *p) {
  dx_regex *left = parse_concat(p);
  if (p->err)
    return left;

  while (p_peek(p) == '|') {
    p_get(p); // consume '|'
    dx_regex *right = parse_concat(p);
    if (p->err) {
      dx_free_internal(left);
      return right;
    }
    left = smart_alt(left, right);
  }
  return left;
}

// -----------------------------
// Public API
// -----------------------------

dx_regex *dx_compile(const char *pattern, char **error_msg) {
  if (error_msg)
    *error_msg = NULL;
  if (!pattern) {
    if (error_msg) {
      const char *msg = "Pattern is NULL";
      *error_msg = (char *)malloc(strlen(msg) + 1);
      if (*error_msg)
        strcpy(*error_msg, msg);
    }
    return NULL;
  }

  Parser p = {0};
  p.s = pattern;
  p.i = 0;
  p.err = NULL;

  dx_regex *ast = NULL;
  if (pattern[0] == '\0') {
    // Empty pattern -> ε
    ast = mk_eps();
  } else {
    ast = parse_alt(&p);
    if (!p.err && !p_eof(&p)) {
      p_set_error(&p, "unexpected character '%c'", p_peek(&p));
    }
  }

  if (p.err) {
    if (error_msg) {
      *error_msg = p.err;
    } else {
      free(p.err);
    }
    dx_free_internal(ast);
    return NULL;
  }
  return ast;
}

int dx_match_full(const dx_regex *re, const char *input) {
  if (!re || !input)
    return 0;

  dx_regex *cur = clone_regex(re); // work on a private copy
  for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
    dx_regex *next = derive(cur, *p);
    dx_free_internal(cur);
    cur = next;
  }
  int ans = is_nullable(cur);
  dx_free_internal(cur);
  return ans ? 1 : 0;
}

int dx_search(const dx_regex *re, const char *input) {
  if (!re || !input)
    return 0;

  // If the pattern can match the empty string, it matches at position 0.
  if (is_nullable(re))
    return 1;

  size_t n = strlen(input);
  for (size_t i = 0; i < n; i++) {
    dx_regex *cur = clone_regex(re);
    for (size_t j = i; j < n; j++) {
      dx_regex *next = derive(cur, (unsigned char)input[j]);
      dx_free_internal(cur);
      cur = next;
      if (is_nullable(cur)) {
        dx_free_internal(cur);
        return 1;
      }
    }
    dx_free_internal(cur);
  }
  return 0;
}
