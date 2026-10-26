/*
 * stubs.h - support for the generated stubs (stubs.c)
 */

#ifndef STUBS_H
#define STUBS_H

void StubLog(const char *name);

// logs the first call of a not yet translated function
#define STUB() { static int called; if (!called) { called = 1; StubLog(__FUNCTION__); } }

#endif
