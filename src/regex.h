#ifndef DX_REGEX_H
#define DX_REGEX_H

#ifdef __cplusplus
extern "C" {
#endif

// Minimal public header for a tiny derivative-based regex engine.
//
// Supported syntax (intentionally small and simple):
// - Literals: a b c ... (any visible ASCII character except special operators)
// - Concatenation: AB (implicit by adjacency)
// - Alternation: A|B
// - Kleene star: A*
// - Grouping: (A)
// - Character class: [abc] (a set of literal characters; ranges like a-z are
// NOT supported)
// - Escapes: use backslash to escape metacharacters inside and outside classes:
//   \\, \|, \*, \(, \), \[, \], and \- (for literal dash inside a class)
// Notes:
// - There are no anchors (^, $), dot (.), plus (+), or question mark (?) in
// this tiny engine.
// - Whitespace is treated as a literal unless escaped or inside a class where
// it's also literal.
// - The implementation uses Brzozowski derivatives with simple structural
// sharing and simplification.

// Opaque compiled regex type.
typedef struct dx_regex dx_regex;

// Compile a pattern into an internal regex representation.
// On failure, returns NULL and, if error_msg is non-NULL, sets *error_msg to a
// malloc'd C-string describing the error. Caller is responsible for
// free(*error_msg).
dx_regex *dx_compile(const char *pattern, char **error_msg);

// Full match: returns 1 if 'input' matches the pattern entirely, 0 otherwise.
int dx_match_full(const dx_regex *re, const char *input);

// Search: returns 1 if any substring of 'input' matches the pattern (i.e., like
// a naive "find"), 0 otherwise. This is implemented in a very simple way (by
// trying each start position).
int dx_search(const dx_regex *re, const char *input);

// Free a compiled regex.
void dx_free(dx_regex *re);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DX_REGEX_H
