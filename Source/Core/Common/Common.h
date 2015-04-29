// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef __has_feature
#define __has_feature(x) 0
#endif

// Git version number
extern const char *scm_desc_str;
extern const char *scm_branch_str;
extern const char *scm_rev_str;
extern const char *scm_rev_git_str;
extern const char *netplay_dolphin_ver;

// Force enable logging in the right modes. For some reason, something had changed
// so that debugfast no longer logged.
#if defined(_DEBUG) || defined(DEBUGFAST)
#undef LOGGING
#define LOGGING 1
#endif

#if defined(__GNUC__) || __clang__
// Disable "unused function" warnings for the ones manually marked as such.
#define UNUSED __attribute__((unused))
#else
// Not sure MSVC even checks this...
#define UNUSED
#endif

// An inheritable class to disallow the copy constructor and operator= functions
class NonCopyable
{
protected:
	NonCopyable() {}
	NonCopyable(const NonCopyable&&) {}
	void operator=(const NonCopyable&&) {}
private:
	NonCopyable(NonCopyable&);
	NonCopyable& operator=(NonCopyable& other);
};

#if defined _WIN32

// Alignment
	#define GC_ALIGNED16(x) __declspec(align(16)) x
	#define GC_ALIGNED32(x) __declspec(align(32)) x
	#define GC_ALIGNED64(x) __declspec(align(64)) x
	#define GC_ALIGNED128(x) __declspec(align(128)) x
	#define GC_ALIGNED16_DECL(x) __declspec(align(16)) x
	#define GC_ALIGNED64_DECL(x) __declspec(align(64)) x

// Since they are always around on Windows
	#define HAVE_WX 1
	// FIXME
	//#define HAVE_OPENAL 1

	#define HAVE_PORTAUDIO 1

#endif

// Windows compatibility
#ifndef _WIN32
#include <limits.h>
#define MAX_PATH PATH_MAX

#define __forceinline inline __attribute__((always_inline))
#define GC_ALIGNED16(x) __attribute__((aligned(16))) x
#define GC_ALIGNED32(x) __attribute__((aligned(32))) x
#define GC_ALIGNED64(x) __attribute__((aligned(64))) x
#define GC_ALIGNED128(x) __attribute__((aligned(128))) x
#define GC_ALIGNED16_DECL(x) __attribute__((aligned(16))) x
#define GC_ALIGNED64_DECL(x) __attribute__((aligned(64))) x
#endif

#ifdef _MSC_VER
#define __strdup _strdup
#define __getcwd _getcwd
#define __chdir _chdir
#else
#define __strdup strdup
#define __getcwd getcwd
#define __chdir chdir
#endif

// Dummy macro for marking translatable strings that can not be immediately translated.
// wxWidgets does not have a true dummy macro for this.
#define _trans(a) a

// Host communication.
enum HOST_COMM
{
	// Begin at 10 in case there is already messages with wParam = 0, 1, 2 and so on
	WM_USER_STOP = 10,
	WM_USER_CREATE,
	WM_USER_SETCURSOR,
};

// Used for notification on emulation state
enum EMUSTATE_CHANGE
{
	EMUSTATE_CHANGE_PLAY = 1,
	EMUSTATE_CHANGE_PAUSE,
	EMUSTATE_CHANGE_STOP
};

#include "Common/CommonTypes.h" // IWYU pragma: export
#include "Common/CommonFuncs.h" // IWYU pragma: export // NOLINT
#include "Common/MsgHandler.h" // IWYU pragma: export
#include "Common/Logging/Log.h" // IWYU pragma: export
