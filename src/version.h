/*
 * FS utility functions
 * Created by LoRd_MuldeR <mulder2@gmx.de>.
 * 
 * This work is licensed under the CC0 1.0 Universal License.
 * To view a copy of the license, visit:
 * https://creativecommons.org/publicdomain/zero/1.0/legalcode
 */

#pragma once

///////////////////////////////////////////////////////////////////////////////
// Version Info
///////////////////////////////////////////////////////////////////////////////

#define VER_MSLEEP_MAJOR      1
#define VER_MSLEEP_MINOR_HI   0
#define VER_MSLEEP_MINOR_LO   6
#define VER_MSLEEP_PATCH      0

///////////////////////////////////////////////////////////////////////////////
// Helper macros (aka: having fun with the C pre-processor)
///////////////////////////////////////////////////////////////////////////////

#if (VER_MSLEEP_PATCH > 0)
#define ___VER_MSLEEP_STR___(X)       #X
#define __VER_MSLEEP_STR__(W,X,Y,Z)   ___VER_MSLEEP_STR___(v##W.X##Y-Z)
#define _VER_MSLEEP_STR_(W,X,Y,Z)     __VER_MSLEEP_STR__(W,X,Y,Z)
#define VER_MSLEEP_STR                _VER_MSLEEP_STR_(VER_MSLEEP_MAJOR,VER_MSLEEP_MINOR_HI,VER_MSLEEP_MINOR_LO,VER_MSLEEP_PATCH)
#else
#define ___VER_MSLEEP_STR___(X)       #X
#define __VER_MSLEEP_STR__(W,X,Y)     ___VER_MSLEEP_STR___(v##W.X##Y)
#define _VER_MSLEEP_STR_(W,X,Y)       __VER_MSLEEP_STR__(W,X,Y)
#define VER_MSLEEP_STR                _VER_MSLEEP_STR_(VER_MSLEEP_MAJOR,VER_MSLEEP_MINOR_HI,VER_MSLEEP_MINOR_LO)
#endif
