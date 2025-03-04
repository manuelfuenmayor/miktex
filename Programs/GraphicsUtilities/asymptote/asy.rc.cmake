/**
 * @file asy.rc
 * @author Christian Schenk
 * @brief Windows resources
 *
 * @copyright Copyright © 2018-2022 Christian Schenk
 *
 * This file is free software; the copyright holder gives unlimited permission
 * to copy and/or distribute it, with or without modifications, as long as this
 * notice is preserved.
 */

#ifdef RC_INVOKED
IDI_ICON1 ICON DISCARDABLE "source/asy.ico"
#endif

#include "asy-version.h"

#define VER_FILEDESCRIPTION_STR "@MIKTEX_COMP_DESCRIPTION@"
#define VER_INTERNALNAME_STR "asy"
#define VER_ORIGINALFILENAME_STR "miktex-asy.exe"

#include "miktex/win/version.rc"
