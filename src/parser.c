#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

char p_peek(Parser *p) { return p->s[p->i]; }

char p_get(Parser *p) {
  char c = p->s[p->i];
  if (c != '\0')
    p->i++;
  return c;
}

bool p_eof(Parser *p) { return p->s[p->i] == '\0'; }

void p_set_error(Parser *p, const char *fmt, ...) {
  if (p->err)
    return;
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

bool is_atom_start(char c) {
  if (c == '\0')
    return false;
  if (c == '|' || c == ')' || c == '*')
    return false;
  return true;
}

dx_regex *parse_class(Parser *p) {
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

dx_regex *parse_atom(Parser *p) {
  char c = p_peek(p);
  if (c == '(') {
    p_get(p);
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
  } else if (c == '[') {
    p_get(p);
    return parse_class(p);
  } else if (c == '\\') {
    p_get(p);
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

dx_regex *parse_unary(Parser *p) {
  dx_regex *a = parse_atom(p);
  if (p->err)
    return a;
  while (p_peek(p) == '*') {
    p_get(p);
    a = smart_star(a);
  }
  return a;
}

dx_regex *parse_concat(Parser *p) {
  dx_regex *left = NULL;
  while (is_atom_start(p_peek(p))) {
    dx_regex *u = parse_unary(p);
    if (p->err) {
      dx_free_internal(left);
      return u;
    }
    if (!left) {
      left = u;
    } else {
      left = smart_concat(left, u);
    }
  }
  if (!left) {
    left = mk_eps();
  }
  return left;
}

dx_regex *parse_alt(Parser *p) {
  dx_regex *left = parse_concat(p);
  if (p->err)
    return left;

  while (p_peek(p) == '|') {
    p_get(p);
    dx_regex *right = parse_concat(p);
    if (p->err) {
      dx_free_internal(left);
      return right;
    }
    left = smart_alt(left, right);
  }
  return left;
}
