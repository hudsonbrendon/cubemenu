#pragma once
/* key=value lines, '#' comments; parses in place (the buffer must outlive the ini_t) */
#define INI_MAX 640

typedef struct { const char *k[INI_MAX], *v[INI_MAX]; int n; } ini_t;

void ini_parse(ini_t *ini, char *buf);
const char *ini_get(const ini_t *ini, const char *key);
int ini_get_int(const ini_t *ini, const char *key, int def);
