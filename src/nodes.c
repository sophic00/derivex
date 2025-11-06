#include "internal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static struct dx_regex DX_NULL_NODE = {.kind = R_NULL};
static struct dx_regex DX_EPS_NODE = {.kind = R_EPS};

dx_regex *mk_null(void) { return &DX_NULL_NODE; }
dx_regex *mk_eps(void) { return &DX_EPS_NODE; }

dx_regex *mk_char(unsigned char c) {
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r)
    return mk_null();
  r->kind = R_CHAR;
  r->u.ch = c;
  return r;
}

dx_regex *mk_class_from_set(const bool set[256], int count) {
  if (count <= 0)
    return mk_null();
  if (count == 1) {
    for (int i = 0; i < 256; i++)
      if (set[i])
        return mk_char((unsigned char)i);
  }
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r)
    return mk_null();
  r->kind = R_CLASS;
  memcpy(r->u.cls.set, set, 256 * sizeof(bool));
  r->u.cls.count = count;
  return r;
}

dx_regex *mk_alt(dx_regex *a, dx_regex *b) {
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r)
    return mk_null();
  r->kind = R_ALT;
  r->u.pair.a = a;
  r->u.pair.b = b;
  return r;
}

dx_regex *mk_concat(dx_regex *a, dx_regex *b) {
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r)
    return mk_null();
  r->kind = R_CONCAT;
  r->u.pair.a = a;
  r->u.pair.b = b;
  return r;
}

dx_regex *mk_star(dx_regex *sub) {
  dx_regex *r = (dx_regex *)malloc(sizeof(dx_regex));
  if (!r)
    return mk_null();
  r->kind = R_STAR;
  r->u.sub = sub;
  return r;
}
