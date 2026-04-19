#pragma once
#include <stdio.h>
#include <string.h>

static int _tests_passed = 0;
static int _tests_failed = 0;

#define ASSERT_TRUE(expr) do { \
    if (expr) { \
        _tests_passed++; \
    } else { \
        fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        _tests_failed++; \
    } \
} while (0)

#define ASSERT_FALSE(expr)      ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b)         ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b)         ASSERT_TRUE((a) != (b))
#define ASSERT_NULL(p)          ASSERT_TRUE((p) == NULL)
#define ASSERT_NOT_NULL(p)      ASSERT_TRUE((p) != NULL)
#define ASSERT_MEM_EQ(a, b, n)  ASSERT_TRUE(memcmp((a), (b), (n)) == 0)
#define ASSERT_STR_EQ(a, b)     ASSERT_TRUE(strcmp((a), (b)) == 0)
