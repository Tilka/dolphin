// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifndef USE_VTUNE
#define INTEL_NO_ITTNOTIFY_API
#endif

#include <ittnotify.h>
#include <jitprofiling.h>

#ifdef _MSC_VER
#pragma comment(lib, "libittnotify.lib")
#pragma comment(lib, "jitprofiling.lib")
#endif

class VTuneTask
{
public:
	VTuneTask(__itt_domain* dom, __itt_string_handle* handle) : domain(dom)
	{
		__itt_task_begin(domain, __itt_null, __itt_null, handle);
	}

	~VTuneTask()
	{
		__itt_task_end(domain);
	}

private:
	__itt_domain* domain;
};
