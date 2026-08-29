# Day 23 — 김현수 원문

출처: https://app.notion.com/e202ae70087183069efb01f1c925b65d

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Stores the source string and delimiter.
typedef struct
{
    const char *source;
    const char delimiter;
} Tokenizer;

// Points to one token in the source string.
typedef struct
{
    const char *start;
    size_t length;
} Token;

// Prints the source and delimiter.
static void print_tokenizer_info(const Tokenizer *tokenizer);
// Finds the next token.
static bool next_token(const Tokenizer *tokenizer, Token *token);
// Prints every token.
static void print_all_tokens(const Tokenizer *tokenizer);
// Prints one token.
static void print_token(const Token *token, size_t token_number);
// Checks that the source is unchanged.
static void verify_source_unchanged(const Tokenizer *tokenizer, const char *expected_source);

int main(void)
{
    const char source[] = "GPS,37.5665,126.9780,20260213";
    const char expected_source[] = "GPS,37.5665,126.9780,20260213";

    const Tokenizer tokenizer = {
        .source = source,
        .delimiter = ',',
    };
    printf("=== Day 23: Safe String Tokenizer (static) ===\n\n");

    print_tokenizer_info(&tokenizer);
    print_all_tokens(&tokenizer);
    verify_source_unchanged(&tokenizer, expected_source);

    return 0;
}

static void print_tokenizer_info(const Tokenizer *tokenizer)
{
    assert(tokenizer != NULL);
    assert(tokenizer->source != NULL);

    printf("Input Data: \"%s\"\n", tokenizer->source);
    printf("Delimiter : '%c'\n\n", tokenizer->delimiter);
}

static bool next_token(const Tokenizer *tokenizer, Token *token)
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

static void print_all_tokens(const Tokenizer *tokenizer)
{
    assert(tokenizer != NULL);
    Token token;
    size_t token_number = 0;
    bool has_token = next_token(tokenizer, &token);
    while (has_token)
    {
        print_token(&token, ++token_number);
        // NULL because next_token keeps static variables.
        // Continue from the cursor saved by the previous call.
        has_token = next_token(NULL, &token);
    }
    putchar('\n');
}

static void print_token(const Token *token, size_t token_number)
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

static void verify_source_unchanged(const Tokenizer *tokenizer, const char *expected_source)
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


````
