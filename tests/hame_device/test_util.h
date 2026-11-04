// Minimal assertion helpers for the host tests.
#pragma once

#include <cstdio>
#include <string>

static int g_failures = 0;

#define EXPECT(cond)                                                     \
  do {                                                                   \
    if (!(cond)) {                                                       \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
      g_failures++;                                                      \
    }                                                                    \
  } while (0)

#define EXPECT_EQ(a, b)                                                                     \
  do {                                                                                      \
    auto _a = (a);                                                                          \
    auto _b = (b);                                                                          \
    if (!(_a == _b)) {                                                                      \
      printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);                         \
      g_failures++;                                                                         \
    }                                                                                       \
  } while (0)

#define EXPECT_STR(a, b)                                                                    \
  do {                                                                                      \
    std::string _a = (a);                                                                   \
    std::string _b = (b);                                                                   \
    if (_a != _b) {                                                                         \
      printf("FAIL %s:%d: %s\n  got:      \"%s\"\n  expected: \"%s\"\n", __FILE__, __LINE__, \
             #a, escape(_a).c_str(), escape(_b).c_str());                                   \
      g_failures++;                                                                         \
    }                                                                                       \
  } while (0)

static inline std::string escape(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (c == '\r')
      out += "\\r";
    else if (c == '\n')
      out += "\\n";
    else if (static_cast<unsigned char>(c) < 0x20)
      out += "\\x" + std::to_string(static_cast<unsigned char>(c));
    else
      out += c;
  }
  return out;
}

static inline int finish() {
  if (g_failures == 0)
    printf("all passed\n");
  return g_failures == 0 ? 0 : 1;
}
