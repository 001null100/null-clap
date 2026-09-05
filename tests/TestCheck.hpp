#pragma once

#include <cstdio>
#include <cstdlib>

// Do not change NDEBUG for just a test translation unit: Plugin's helper base
// depends on it and must match the linked framework. Check explicitly instead.
#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            std::abort(); \
        } \
    } while (false)
