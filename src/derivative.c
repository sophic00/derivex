#include "internal.h"

int is_nullable(const dx_regex *r) {
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

dx_regex *derive(const dx_regex *r, unsigned char c) {
  switch (r->kind) {
  case R_NULL:
  case R_EPS:
    return mk_null();
  case R_CHAR:
    return (r->u.ch == c) ? mk_eps() : mk_null();
  case R_CLASS:
    return r->u.cls.set[c] ? mk_eps() : mk_null();
  case R_ALT: {
    dx_regex *da = derive(r->u.pair.a, c);
    dx_regex *db = derive(r->u.pair.b, c);
    return smart_alt(da, db);
  }
  case R_CONCAT: {
    dx_regex *dA = derive(r->u.pair.a, c);
    dx_regex *Bclone = clone_regex(r->u.pair.b);
    dx_regex *term1 = smart_concat(dA, Bclone);
    if (is_nullable(r->u.pair.a)) {
      dx_regex *dB = derive(r->u.pair.b, c);
      return smart_alt(term1, dB);
    } else
      return term1;
  }
  case R_STAR: {
    dx_regex *dA = derive(r->u.sub, c);
    dx_regex *Astar = smart_star(clone_regex(r->u.sub));
    return smart_concat(dA, Astar);
  }
  }
  return mk_null();
}
