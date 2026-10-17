#include "ini.h"
#include <stdlib.h>
#include <string.h>

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) *--e = 0;
    return s;
}

void ini_parse(ini_t *ini, char *buf) {
    ini->n = 0;
    char *line = buf;
    while (line && *line && ini->n < INI_MAX) {
        char *next = strchr(line, '\n');
        if (next) *next++ = 0;
        char *eq = strchr(line, '=');
        if (line[0] != '#' && line[0] != '[' && eq) {
            *eq = 0;
            ini->k[ini->n] = trim(line);
            ini->v[ini->n] = trim(eq + 1);
            ini->n++;
        }
        line = next;
    }
}

const char *ini_get(const ini_t *ini, const char *key) {
    for (int i = 0; i < ini->n; i++) if (!strcmp(ini->k[i], key)) return ini->v[i];
    return NULL;
}

int ini_get_int(const ini_t *ini, const char *key, int def) {
    const char *v = ini_get(ini, key);
    return v && *v ? atoi(v) : def;
}
