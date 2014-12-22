// RUN: %clang_cc1 -DSTDCPP11 -std=c++11 -verify -fsyntax-only %s
// RUN: %clang_cc1 -DSTDGNU11 -std=gnu++11 -verify -fsyntax-only %s
// RUN: %clang_cc1 -DSTDGNU11TRI -trigraphs -std=gnu++11 -verify -fsyntax-only %s
// RUN: %clang_cc1 -DSTDCPP17 -std=c++1z -verify -fsyntax-only %s
// RUN: %clang_cc1 -DSTDCPP17TRI -trigraphs -std=c++1z -verify -fsyntax-only %s

void foo() {
#if defined(NOFLAGS) || defined(STDCPP11) || defined(STDGNU11TRI) || defined(STDCPP17TRI)
  const char c[] = "??/n"; // expected-warning{{trigraph converted to '\' character}}
#elif defined(STDGNU11) || defined(STDCPP17)
  const char c[] = "??/n"; // expected-warning{{trigraph ignored}}
#else
#error Not handled.
#endif
}
