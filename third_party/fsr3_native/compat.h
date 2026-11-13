// Linux equivalents for the SDK's bounded MSVC string helpers.
#pragma once
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cstdlib>
#include <climits>
#include <locale>
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
inline int wcscpy_s(wchar_t *d, size_t n, const wchar_t *s) {
    if (!d || !s || !n || std::wcslen(s) >= n) { if (d && n) *d=0; return 1; }
    std::wcscpy(d,s); return 0;
}
template<size_t N> int wcscpy_s(wchar_t (&d)[N], const wchar_t *s) { return wcscpy_s(d,N,s); }
inline int strcpy_s(char *d, size_t n, const char *s) {
    if (!d || !s || !n || std::strlen(s) >= n) { if (d && n) *d=0; return 1; }
    std::strcpy(d,s); return 0;
}
#include <cmath>
#include <new>
#include <utility>
// [XCDE] T-FSR3-LUT: libstdc++'s <math.h>/<stdlib.h> wrappers put the std::abs overloads in the
// global namespace. Without them the SDK's abs(float) in lanczos2() is the C abs(int), which
// truncates every |x| < 1 to 0 and fills the first half of the Lanczos LUT with 1.0: the LUT
// history filter then lost stationary detail (the reason for the analytical-Lanczos workaround).
// compat_windows.h already carried this fix for the Windows Clang/libstdc++ build.
#include <stdlib.h>
#include <math.h>
template<class... A> int sprintf_s(char *d, size_t n, const char *f, A... a) { return std::snprintf(d,n,f,a...); }
template<size_t N, class... A> int sprintf_s(char (&d)[N], const char *f, A... a) { return std::snprintf(d,N,f,a...); }

#include <FidelityFX/host/ffx_util.h>
