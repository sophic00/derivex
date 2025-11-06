#ifndef DX_REGEX_H
#define DX_REGEX_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dx_regex dx_regex;

dx_regex *dx_compile(const char *pattern, char **error_msg);

int dx_match_full(const dx_regex *re, const char *input);

int dx_search(const dx_regex *re, const char *input);

void dx_free(dx_regex *re);

#ifdef __cplusplus
}
#endif

#endif
