#include <stdio.h>
#include <stdlib.h>

#include "regex.h"

static void usage(const char *argv0) {
  fprintf(stderr, "Usage: %s PATTERN STRING\n", argv0);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    usage(argv[0]);
    return 2;
  }

  const char *pattern = argv[1];
  const char *text = argv[2];

  char *err = NULL;
  dx_regex *re = dx_compile(pattern, &err);
  if (!re) {
    fprintf(stderr, "Compile error: %s\n", err ? err : "(unknown)");
    free(err);
    return 2;
  }
  free(err);

  // Substring search semantics: returns MATCH if any substring matches PATTERN.
  int matched = dx_search(re, text);

  printf("%s\n", matched ? "MATCH" : "NO MATCH");

  dx_free(re);
  return matched ? 0 : 1;
}
