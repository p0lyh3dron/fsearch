
#pragma once

#include <pcre.h>
#include <stdbool.h>
#include <stdint.h>
#include <unicode/ucasemap.h>
#include <unicode/unorm2.h>

#include "fsearch_query_flags.h"
#include "fsearch_utf.h"

#define OVECCOUNT 3

typedef struct FsearchToken {

    char *text;
    size_t text_len;

    uint32_t has_separator;
    uint32_t (*search_func)(const char *,
                            const char *,
                            void *token,
                            FsearchUtfConversionBuffer *buffer);

    UCaseMap *case_map;
    const UNormalizer2 *normalizer;

    FsearchUtfConversionBuffer *needle_buffer;

    uint32_t fold_options;

    pcre *regex;
    pcre_extra *regex_study;
    int ovector[OVECCOUNT];

    int32_t is_utf;
} FsearchToken;

FsearchToken **
fsearch_tokens_new(const char *query, FsearchQueryFlags flags);

void
fsearch_tokens_free(FsearchToken **tokens);

