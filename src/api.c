#include <stdlib.h>
#include <string.h>

#include "internal.h"

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
  dx_regex *cur = clone_regex(re);
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
