#include "internal.h"
#include <stdlib.h>
#include <string.h>

void dx_free_internal(dx_regex *r) {
  if (!r)
    return;
  switch (r->kind) {
  case R_NULL:
  case R_EPS:
    return;
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

dx_regex *smart_alt(dx_regex *a, dx_regex *b) {
  if (a->kind == R_NULL) {
    return b;
  }
  if (b->kind == R_NULL) {
    return a;
  }
  if (a == b) {
    dx_free_internal(b);
    return a;
  }
  return mk_alt(a, b);
}

dx_regex *smart_concat(dx_regex *a, dx_regex *b) {
  if (a->kind == R_NULL || b->kind == R_NULL) {
    dx_free_internal(a);
    dx_free_internal(b);
    return mk_null();
  }
  if (a->kind == R_EPS) {
    dx_free_internal(a);
    return b;
  }
  if (b->kind == R_EPS) {
    dx_free_internal(b);
    return a;
  }
  return mk_concat(a, b);
}

dx_regex *smart_star(dx_regex *a) {
  if (a->kind == R_NULL || a->kind == R_EPS) {
    dx_free_internal(a);
    return mk_eps();
  }
  if (a->kind == R_STAR) {
    return a;
  }
  return mk_star(a);
}

dx_regex *clone_regex(const dx_regex *r) {
  if (!r)
    return NULL;
  switch (r->kind) {
  case R_NULL:
    return mk_null();
  case R_EPS:
    return mk_eps();
  case R_CHAR:
    return mk_char(r->u.ch);
  case R_CLASS:
    return mk_class_from_set(r->u.cls.set, r->u.cls.count);
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
