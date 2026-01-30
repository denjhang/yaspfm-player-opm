/* See LICENSE for licence details. */
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#define _XOPEN_SOURCE 600
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

enum misc_t {
	BITS_PER_BYTE  = 8,
	SELECT_TIMEOUT = 15000, /* usec */
};

/* SPFM Light slot: 0x00 or 0x01 */
#define OPM_SLOT_NUM   0x00
#define OPNA_SLOT_NUM  0x01

extern volatile sig_atomic_t catch_sigint;
