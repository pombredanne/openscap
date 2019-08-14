/*
 * Copyright 2017 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      Jan Černý <jcerny@redhat.com>
 */

#ifndef OSCAP_COMPAT_H_
#define OSCAP_COMPAT_H_

#include "oscap_export.h"
#include "oscap_platforms.h"

/* Fallback functions fixing portability issues */

#ifndef HAVE_STRSEP
char *strsep(char **stringp, const char *delim);
#endif

#ifndef HAVE_STRPTIME
#include <time.h>
char *strptime(const char *buf, const char *format, struct tm *tm);
#endif

#if defined(unix) || defined(__unix__) || defined(__unix)
#define OSCAP_UNIX
#endif

#ifdef OS_WINDOWS
#define PATH_MAX _MAX_PATH
#endif

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#define __PRETTY_FUNCTION__ __FUNCTION__
#define __attribute__(x)

/* Definitions for access() */
#define F_OK 0
#define W_OK 2
#define R_OK 4

#endif

#if !defined(HAVE_DEV_TO_TTY) && !defined(OS_WINDOWS)
#include <sys/types.h>
#define ABBREV_DEV  1     /* remove /dev/         */
#define ABBREV_TTY  2     /* remove tty           */
#define ABBREV_PTS  4     /* remove pts/          */

extern unsigned dev_to_tty(char *__restrict ret, unsigned chop, dev_t dev_t_dev, int pid, unsigned int flags);
#endif

#endif
