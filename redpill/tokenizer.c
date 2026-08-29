/* 김현수 제출 코드 기반. 원문: originals/day23.md */
#include "tokenizer.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Stores the source string and delimiter.


// Points to one token in the source string.


// Prints the source and delimiter.
static void print_tokenizer_info(const rp23_Tokenizer *tokenizer);
// Finds the next token.
bool rp23_next_token(const rp23_Tokenizer *tokenizer, rp23_Token *token);
// Prints every token.
static void print_all_tokens(const rp23_Tokenizer *tokenizer);
// Prints one token.
static void print_token(const rp23_Token *token, size_t token_number);
// Checks that the source is unchanged.
static void verify_source_unchanged(const rp23_Tokenizer *tokenizer, const char *expected_source);

int rp23_demo(void)
{
    /* Static cursor must never outlive this demo's source storage. */
    static const char source[] = "GPS,37.5665,126.9780,20260213";
    const char expected_source[] = "GPS,37.5665,126.9780,20260213";

    const rp23_Tokenizer tokenizer = {
        .source = source,
        .delimiter = ',',
    };
    printf("=== Day 23: Safe String Tokenizer (static) ===\n\n");

    print_tokenizer_info(&tokenizer);
    print_all_tokens(&tokenizer);
    verify_source_unchanged(&tokenizer, expected_source);

    return 0;
}

static void print_tokenizer_info(const rp23_Tokenizer *tokenizer)
{
    assert(tokenizer != NULL);
    assert(tokenizer->source != NULL);

    printf("Input Data: \"%s\"\n", tokenizer->source);
    printf("Delimiter : '%c'\n\n", tokenizer->delimiter);
}

bool rp23_next_token(const rp23_Tokenizer *tokenizer, rp23_Token *token)
{
    // Static state preserves the scan position between function calls.
    static const char *cursor = NULL;
    static char active_delimiter = '\0';
    assert(token != NULL);

    // Non-NULL starts a new scan; NULL continues the current scan.
    if (tokenizer != NULL)
    {
        assert(tokenizer->source != NULL);
        assert(tokenizer->delimiter != '\0');
        cursor = tokenizer->source;
        active_delimiter = tokenizer->delimiter;
    }
    // CASE: No string to tokenize
    if (cursor == NULL)
    {
        return false;
    }
    // Match strtok behavior: skip delimiters instead of returning empty tokens.
    while (*cursor == active_delimiter)
    {
        cursor++;
    }

    // CASE: String End
    if (*cursor == '\0')
    {
        cursor = NULL;
        return false;
    }

    // Start the Tokenization
    // 1. Initialize token->start
    token->start = cursor;
    // 2. Advance the cursor until delimiter or null char
    while (*cursor != active_delimiter && *cursor != '\0')
    {
        cursor++;
    }
    // 3. Update token length
    // Both pointers are inside the same source, so their difference is the token length.
    token->length = (size_t)(cursor - token->start);
    return true;
}

static void print_all_tokens(const rp23_Tokenizer *tokenizer)
{
    assert(tokenizer != NULL);
    rp23_Token token;
    size_t token_number = 0;
    bool has_token = rp23_next_token(tokenizer, &token);
    while (has_token)
    {
        print_token(&token, ++token_number);
        // NULL because next_token keeps static variables.
        // Continue from the cursor saved by the previous call.
        has_token = rp23_next_token(NULL, &token);
    }
    putchar('\n');
}

static void print_token(const rp23_Token *token, size_t token_number)
{
    assert(token != NULL);
    assert(token->start != NULL);
    printf("Token %zu: ", token_number);

    for (size_t i = 0; i < token->length; ++i)
    {
        putchar(token->start[i]);
    }
    putchar('\n');
}

static void verify_source_unchanged(const rp23_Tokenizer *tokenizer, const char *expected_source)
{
    assert(tokenizer != NULL);
    assert(tokenizer->source != NULL);
    assert(expected_source != NULL);

    printf(">> Original string check: \"%s\"\n", tokenizer->source);
    if (strcmp(tokenizer->source, expected_source) == 0)
    {
        printf(">> (Original string remains unmodified)\n");
    }
    else
    {
        printf(">> (Original string modified)\n");
    }
}
