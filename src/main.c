#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "regex.h"

static int tests_run = 0;
static int tests_failed = 0;

static void report_fail(const char *kind, const char *pattern,
                        const char *input, int got, int expect,
                        const char *extra) {
  fprintf(stderr,
          "[FAIL] %s: pattern=\"%s\" input=\"%s\" got=%d expect=%d%s%s\n", kind,
          pattern ? pattern : "(null)", input ? input : "(null)", got, expect,
          extra ? " " : "", extra ? extra : "");
}

static void t_full(const char *pattern, const char *input, int expect) {
  tests_run++;
  char *err = NULL;
  dx_regex *re = dx_compile(pattern, &err);
  if (!re) {
    tests_failed++;
    fprintf(stderr, "[FAIL] compile: pattern=\"%s\" error=%s\n",
            pattern ? pattern : "(null)", err ? err : "(none)");
    free(err);
    return;
  }
  int got = dx_match_full(re, input);
  if (got != expect) {
    tests_failed++;
    report_fail("full", pattern, input, got, expect, NULL);
    putchar('F');
  } else {
    putchar('.');
  }
  dx_free(re);
}

static void t_search(const char *pattern, const char *input, int expect) {
  tests_run++;
  char *err = NULL;
  dx_regex *re = dx_compile(pattern, &err);
  if (!re) {
    tests_failed++;
    fprintf(stderr, "[FAIL] compile: pattern=\"%s\" error=%s\n",
            pattern ? pattern : "(null)", err ? err : "(none)");
    free(err);
    return;
  }
  int got = dx_search(re, input);
  if (got != expect) {
    tests_failed++;
    report_fail("search", pattern, input, got, expect, NULL);
    putchar('F');
  } else {
    putchar('.');
  }
  dx_free(re);
}

static void t_compile_err(const char *pattern) {
  tests_run++;
  char *err = NULL;
  dx_regex *re = dx_compile(pattern, &err);
  if (re != NULL || err == NULL) {
    tests_failed++;
    fprintf(stderr, "[FAIL] expected compile error for pattern=\"%s\"\n",
            pattern ? pattern : "(null)");
    if (re)
      dx_free(re);
    if (err)
      free(err);
    putchar('F');
    return;
  }
  // expected error
  free(err);
  putchar('.');
}

static void run_tests(void) {
  // Literals and concatenation
  t_full("a", "", 0);
  t_full("a", "a", 1);
  t_full("ab", "ab", 1);
  t_full("ab", "a", 0);
  t_search("ab", "xxabyy", 1);
  t_search("ab", "aaby", 1);

  // Alternation
  t_full("a|b", "a", 1);
  t_full("a|b", "b", 1);
  t_full("a|b", "c", 0);
  t_search("a|b", "zzz", 0);
  t_search("a|b", "zaz", 1);

  // Kleene star
  t_full("a*", "", 1);
  t_full("a*", "a", 1);
  t_full("a*", "aaaa", 1);
  t_full("a*b", "b", 1);
  t_full("a*b", "aaab", 1);
  t_full("a*b", "aaac", 0);
  t_full("(ab)*", "", 1);
  t_full("(ab)*", "ab", 1);
  t_full("(ab)*", "abab", 1);
  t_full("(ab)*", "aba", 0);

  // Character class (no ranges, duplicates ok)
  t_full("[abc]", "a", 1);
  t_full("[abc]", "b", 1);
  t_full("[abc]", "d", 0);
  t_search("[abc]", "xyz", 0);
  t_search("[abc]", "xyza", 1);

  // Escapes outside class
  t_full("\\*", "*", 1);
  t_full("\\|", "|", 1);
  t_full("\\(", "(", 1);
  t_full("\\)", ")", 1);
  t_full("\\[", "[", 1);
  t_full("\\]", "]", 1);
  t_full("\\\\", "\\", 1);

  // Escapes inside class (treat escaped char literally)
  t_full("[\\]]", "]", 1);
  t_full("[\\-]", "-", 1);
  t_full("[\\[]", "[", 1);
  t_full("[\\*]", "*", 1);

  // Empty pattern is epsilon
  t_full("", "", 1);
  t_full("", "a", 0);
  t_search("", "", 1);    // epsilon matches at pos 0
  t_search("", "abc", 1); // epsilon matches any string

  // A few composite checks
  t_full("(a|bc)*", "", 1);
  t_full("(a|bc)*", "a", 1);
  t_full("(a|bc)*", "bc", 1);
  t_full("(a|bc)*", "abcbc", 1);
  t_full("(a|bc)*", "abcbca", 1);
  t_full("(a|bc)*", "abcbcab", 0);

  // Invalid patterns (unbalanced, dangling escape)
  t_compile_err("(");
  t_compile_err("[");
  t_compile_err("[abc"); // unterminated class
  t_compile_err("\\");   // dangling escape

  // Class edge-case: empty class -> matches nothing
  t_full("[]", "", 0);
  t_search("[]", "anything", 0);

  // End
  putchar('\n');
  if (tests_failed == 0) {
    printf("All %d tests passed.\n", tests_run);
  } else {
    printf("%d/%d tests failed.\n", tests_failed, tests_run);
  }
}

static void usage(const char *argv0) {
  fprintf(
      stderr,
      "Derivative-based tiny regex demo\n"
      "Usage:\n"
      "  %s --test                 Run built-in tests\n"
      "  %s PATTERN [--full|-f] STRING\n"
      "  %s PATTERN [--full|-f]    Read lines from stdin and test each\n"
      "\n"
      "Syntax:\n"
      "  Literals: a b c ... (most visible ASCII)\n"
      "  Concatenation: AB (implicit)\n"
      "  Alternation: A|B\n"
      "  Kleene star: A*\n"
      "  Grouping: (A)\n"
      "  Character class: [abc] (no ranges)\n"
      "  Escapes: \\\\ \\| \\* \\( \\) \\[ \\] and \\- (inside class)\n"
      "\n"
      "By default uses a substring search. Use --full or -f for full match.\n",
      argv0, argv0, argv0);
}

static void rstrip_newline(char *s) {
  if (!s)
    return;
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
    s[--n] = '\0';
  }
}

int main(int argc, char **argv) {
  if (argc <= 1 || (argc >= 2 && strcmp(argv[1], "--test") == 0)) {
    run_tests();
    return tests_failed == 0 ? 0 : 1;
  }

  const char *pattern = NULL;
  const char *text = NULL;
  int full = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--full") == 0 || strcmp(argv[i], "-f") == 0) {
      full = 1;
    } else if (!pattern) {
      pattern = argv[i];
    } else if (!text) {
      text = argv[i];
    } else {
      // ignore extras
    }
  }

  if (!pattern) {
    usage(argv[0]);
    return 2;
  }

  char *err = NULL;
  dx_regex *re = dx_compile(pattern, &err);
  if (!re) {
    fprintf(stderr, "Compile error: %s\n", err ? err : "(unknown)");
    free(err);
    return 2;
  }
  free(err);

  int exit_code = 0;

  if (text) {
    int matched = full ? dx_match_full(re, text) : dx_search(re, text);
    printf("%s\n", matched ? "MATCH" : "NO MATCH");
    exit_code = matched ? 0 : 1;
  } else {
    // Read lines from stdin, print "1<TAB>line" if match else "0<TAB>line"
    char buf[4096];
    while (fgets(buf, sizeof(buf), stdin)) {
      rstrip_newline(buf);
      int matched = full ? dx_match_full(re, buf) : dx_search(re, buf);
      printf("%d\t%s\n", matched ? 1 : 0, buf);
    }
  }

  dx_free(re);
  return exit_code;
}
