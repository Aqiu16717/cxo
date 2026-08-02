/*
 * lang.c - Language Descriptor Table
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <string.h>
#include "../include/cxo.h"

/* Single source of truth for supported languages.
 * The first entry is the default language (no URL prefix). */
const cxo_lang_t CXO_LANGS[] = {
    { "zh", "",   "zh_CN", "中文" },
    { "en", "en", "en_US", "English" },
};

const size_t CXO_LANG_COUNT = sizeof(CXO_LANGS) / sizeof(CXO_LANGS[0]);

const cxo_lang_t* cxo_lang_find(const char* code)
{
    size_t i;

    if (!code) {
        return NULL;
    }
    for (i = 0; i < CXO_LANG_COUNT; i++) {
        if (strcmp(CXO_LANGS[i].code, code) == 0) {
            return &CXO_LANGS[i];
        }
    }
    return NULL;
}

size_t cxo_lang_index(const char* code)
{
    size_t i;

    if (!code) {
        return 0;
    }
    for (i = 0; i < CXO_LANG_COUNT; i++) {
        if (strcmp(CXO_LANGS[i].code, code) == 0) {
            return i;
        }
    }
    return 0;
}
