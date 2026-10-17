#pragma once
#include <stdbool.h>

/* Swiss DOL on the card, NULL if none */
const char *boot_find_swiss(void);
/* Load a DOL and jump to it with argv = { path, args... }. Only returns on failure. */
bool boot_dol(const char *path, int nargs, const char **args, void (*before_jump)(void), char *err, int errlen);
