///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2005, 2006
// University of Rochester
// Department of Computer Science
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the University of Rochester nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef __HRTIME_H__
#define __HRTIME_H__

#ifdef LINUX
#ifdef X86
// gethrtime implementation by Kai Shen for x86 Linux

#include <stdio.h>
#include <string.h>
#include <assert.h>

// get the number of CPU cycles per microsecond from Linux /proc filesystem
// return < 0 on error
inline double getMHZ_x86(void)
{
    double mhz = -1;
    char line[1024], *s, search_str[] = "cpu MHz";
    FILE* fp;

    // open proc/cpuinfo
    if ((fp = fopen("/proc/cpuinfo", "r")) == NULL)
        return -1;

    // ignore all lines until we reach MHz information
    while (fgets(line, 1024, fp) != NULL) {
        if (strstr(line, search_str) != NULL) {
            // ignore all characters in line up to :
            for (s = line; *s && (*s != ':'); ++s);
            // get MHz number
            if (*s && (sscanf(s+1, "%lf", &mhz) == 1))
                break;
        }
    }

    if (fp != NULL)
        fclose(fp);
    return mhz;
}

// get the number of CPU cycles since startup using rdtsc instruction
inline unsigned long long gethrcycle_x86()
{
    unsigned int tmp[2];

    asm ("rdtsc"
         : "=a" (tmp[1]), "=d" (tmp[0])
         : "c" (0x10) );
    return (((unsigned long long)tmp[0] << 32 | tmp[1]));
}

// get the elapsed time (in nanoseconds) since startup
inline unsigned long long getElapsedTime()
{
    static double CPU_MHZ = 0;
    if (CPU_MHZ == 0)
        CPU_MHZ = getMHZ_x86();
    return (unsigned long long)(gethrcycle_x86() * 1000 / CPU_MHZ);
}
#endif // X86
#endif // LINUX

#ifdef SPARC
// use gethrtime() on SPARC/SOLARIS
inline unsigned long long getElapsedTime()
{
    return gethrtime();
}
#endif // SPARC

#ifdef DARWIN
// x86/DARWIN timer

// This code is based on code available through the Apple developer's
// connection website at http://developer.apple.com/qa/qa2004/qa1398.html

#include <mach/mach_time.h>

inline unsigned long long getElapsedTime()
{
    static mach_timebase_info_data_t sTimebaseInfo;

    // If this is the first time we've run, get the timebase.
    // We can use denom == 0 to indicate that sTimebaseInfo is
    // uninitialised because it makes no sense to have a zero
    // denominator as a fraction.

    if ( sTimebaseInfo.denom == 0 )
        (void)mach_timebase_info(&sTimebaseInfo);

    return mach_absolute_time() * sTimebaseInfo.numer / sTimebaseInfo.denom;
}

#endif

#endif // __HRTIME_H__
