/*
 * Copyright (c) 2019 SK Telecom Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>
#include <ctype.h>
#include <glib.h>

#ifdef HAVE_SYSCALL
#include <sys/syscall.h>
#endif

#if defined(__MSYS__) || defined(_WIN32)
#include <windows.h>
#endif

#ifdef _WIN32
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_INFO 6
#define LOG_DEBUG 7
#define strncasecmp _strnicmp
#else
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "base/nugu_log.h"

#define MAX_LOG_LENGTH 4096
#define MAX_FIELDSIZE_FILENAME 30
#define MAX_FIELDSIZE_FUNCNAME 30

#ifdef NUGU_LOG_USE_ANSICOLOR
#define COLOR_THREAD_ID NUGU_ANSI_COLOR_DARKGRAY
#define COLOR_DEBUG ""
#define COLOR_WARNING NUGU_ANSI_COLOR_LIGHTGRAY
#define COLOR_INFO NUGU_ANSI_COLOR_LIGHTBLUE
#define COLOR_ERROR NUGU_ANSI_COLOR_LIGHTRED
#define COLOR_OFF NUGU_ANSI_COLOR_NORMAL
#else
#define COLOR_THREAD_ID ""
#define COLOR_DEBUG NULL
#define COLOR_WARNING NULL
#define COLOR_INFO NULL
#define COLOR_ERROR NULL
#define COLOR_OFF ""
#endif

#define HEXDUMP_COLUMN_SIZE 32
#define HEXDUMP_LINE_BUFSIZE                                                   \
	(8 + (HEXDUMP_COLUMN_SIZE * 3) + 3 + 3 + (HEXDUMP_COLUMN_SIZE) + 2)

static enum nugu_log_system _log_system = NUGU_LOG_SYSTEM_STDERR;
static enum nugu_log_prefix _log_prefix_fields = NUGU_LOG_PREFIX_DEFAULT;
static enum nugu_log_level _log_level = NUGU_LOG_LEVEL_DEBUG;
static unsigned int _log_module_bitset = NUGU_LOG_MODULE_PRESET_DEFAULT;
static int _log_line_limit = -1;

static nugu_log_handler _log_handler;
static void *_log_handler_user_data;

static int _log_override_enabled;
static int _log_override_checked;

static pthread_mutex_t _log_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct _log_level_info {
	char mark;
	int syslog_level;
	const char *color;
} _log_level_map[] = {
	/* [level] = { mark, syslog_level, color string } */
	[NUGU_LOG_LEVEL_ERROR] = { 'E', LOG_ERR, COLOR_ERROR },
	[NUGU_LOG_LEVEL_WARNING] = { 'W', LOG_WARNING, COLOR_WARNING },
	[NUGU_LOG_LEVEL_INFO] = { 'I', LOG_INFO, COLOR_INFO },
	[NUGU_LOG_LEVEL_DEBUG] = { 'D', LOG_DEBUG, COLOR_DEBUG }
};

#ifdef NUGU_ENV_LOG
static void _log_check_override_system(void)
{
	const char *env;

	env = getenv(NUGU_ENV_LOG);
	if (!env)
		return;

	if (!strncasecmp(env, "stderr", 7)) {
		_log_override_enabled = 1;
		_log_system = NUGU_LOG_SYSTEM_STDERR;
	} else if (!strncasecmp(env, "stdout", 7)) {
		_log_override_enabled = 1;
		_log_system = NUGU_LOG_SYSTEM_STDOUT;
	} else if (!strncasecmp(env, "syslog", 7)) {
		_log_override_enabled = 1;
		_log_system = NUGU_LOG_SYSTEM_SYSLOG;
	} else if (!strncasecmp(env, "none", 5)) {
		_log_override_enabled = 1;
		_log_system = NUGU_LOG_SYSTEM_NONE;
	}
}
#endif

#ifdef NUGU_ENV_LOG_LEVEL
static void _log_check_override_level(void)
{
	const char *env;

	env = getenv(NUGU_ENV_LOG_LEVEL);
	if (!env)
		return;

	if (!strncasecmp(env, "debug", 6))
		_log_level = NUGU_LOG_LEVEL_DEBUG;
	else if (!strncasecmp(env, "info", 5))
		_log_level = NUGU_LOG_LEVEL_INFO;
	else if (!strncasecmp(env, "warning", 8))
		_log_level = NUGU_LOG_LEVEL_WARNING;
	else if (!strncasecmp(env, "error", 6))
		_log_level = NUGU_LOG_LEVEL_ERROR;
	else
		_log_level = NUGU_LOG_LEVEL_DEBUG;
}
#endif

#ifdef NUGU_ENV_LOG_MODULE
static void _log_check_override_module(void)
{
	gchar **modules = NULL;
	const char *env;
	gint count = 0;
	gint i;
	unsigned int bitset = 0;

	env = getenv(NUGU_ENV_LOG_MODULE);
	if (!env)
		return;

	modules = g_strsplit(env, ",", -1);
	if (!modules)
		return;

	count = g_strv_length(modules);
	for (i = 0; i < count; i++) {
		if (!strncasecmp(modules[i], "all", 4)) {
			bitset = NUGU_LOG_MODULE_ALL;
			break;
		}

		if (!strncasecmp(modules[i], "preset_default", 15))
			bitset = bitset | NUGU_LOG_MODULE_PRESET_DEFAULT;
		else if (!strncasecmp(modules[i], "preset_sdk", 11))
			bitset = bitset | NUGU_LOG_MODULE_PRESET_SDK_DEFAULT;
		else if (!strncasecmp(modules[i], "preset_network", 15))
			bitset = bitset | NUGU_LOG_MODULE_PRESET_NETWORK;
		else if (!strncasecmp(modules[i], "default", 8))
			bitset = bitset | NUGU_LOG_MODULE_DEFAULT;
		else if (!strncasecmp(modules[i], "network", 8))
			bitset = bitset | NUGU_LOG_MODULE_NETWORK;
		else if (!strncasecmp(modules[i], "network_trace", 14))
			bitset = bitset | NUGU_LOG_MODULE_NETWORK_TRACE;
		else if (!strncasecmp(modules[i], "protocol", 9))
			bitset = bitset | NUGU_LOG_MODULE_PROTOCOL;
		else if (!strncasecmp(modules[i], "profiling", 10))
			bitset = bitset | NUGU_LOG_MODULE_PROFILING;
		else if (!strncasecmp(modules[i], "audio", 6))
			bitset = bitset | NUGU_LOG_MODULE_AUDIO;
		else if (!strncasecmp(modules[i], "application", 12))
			bitset = bitset | NUGU_LOG_MODULE_APPLICATION;
	}

	_log_module_bitset = bitset;
	g_strfreev(modules);
}
#endif

#ifdef NUGU_ENV_LOG_PROTOCOL_LINE_LIMIT
static void _log_check_override_line_limit(void)
{
	const char *env;
	int value;

	env = getenv(NUGU_ENV_LOG_PROTOCOL_LINE_LIMIT);
	if (!env)
		return;

	value = strtol(env, NULL, 10);
	if (value <= 0)
		_log_line_limit = -1;
	else
		_log_line_limit = value;
}
#endif

#ifdef NUGU_ENV_LOG_PREFIX
static void _log_check_override_prefix(void)
{
	gchar **fields = NULL;
	const char *env;
	gint count = 0;
	gint i;
	unsigned int bitset = 0;

	env = getenv(NUGU_ENV_LOG_PREFIX);
	if (!env)
		return;

	fields = g_strsplit(env, ",", -1);
	if (!fields)
		return;

	count = g_strv_length(fields);
	for (i = 0; i < count; i++) {
		if (!strncasecmp(fields[i], "all", 4)) {
			bitset = NUGU_LOG_PREFIX_ALL;
			break;
		} else if (!strncasecmp(fields[i], "none", 4)) {
			bitset = NUGU_LOG_PREFIX_NONE;
			break;
		}

		if (!strncasecmp(fields[i], "default", 8))
			bitset = bitset | NUGU_LOG_PREFIX_DEFAULT;
		else if (!strncasecmp(fields[i], "timestamp", 10))
			bitset = bitset | NUGU_LOG_PREFIX_TIMESTAMP;
		else if (!strncasecmp(fields[i], "pid", 4))
			bitset = bitset | NUGU_LOG_PREFIX_PID;
		else if (!strncasecmp(fields[i], "tid", 4))
			bitset = bitset | NUGU_LOG_PREFIX_TID;
		else if (!strncasecmp(fields[i], "level", 6))
			bitset = bitset | NUGU_LOG_PREFIX_LEVEL;
		else if (!strncasecmp(fields[i], "filepath", 9))
			bitset = bitset | NUGU_LOG_PREFIX_FILEPATH;
		else if (!strncasecmp(fields[i], "filename", 9))
			bitset = bitset | NUGU_LOG_PREFIX_FILENAME;
		else if (!strncasecmp(fields[i], "function", 9))
			bitset = bitset | NUGU_LOG_PREFIX_FUNCTION;
		else if (!strncasecmp(fields[i], "line", 5))
			bitset = bitset | NUGU_LOG_PREFIX_LINE;
	}

	_log_prefix_fields = bitset;
	g_strfreev(fields);
}
#endif

static void _log_check_override(void)
{
	if (_log_override_checked)
		return;

	_log_override_checked = 1;

#ifdef NUGU_ENV_LOG
	_log_check_override_system();
	if (_log_system == NUGU_LOG_SYSTEM_SYSLOG)
		_log_prefix_fields = NUGU_LOG_PREFIX_NONE;
#endif

#ifdef NUGU_ENV_LOG_LEVEL
	_log_check_override_level();
#endif

#ifdef NUGU_ENV_LOG_MODULE
	_log_check_override_module();
#endif

#ifdef NUGU_ENV_LOG_PROTOCOL_LINE_LIMIT
	_log_check_override_line_limit();
#endif

#ifdef NUGU_ENV_LOG_PREFIX
	_log_check_override_prefix();
#endif
}

static int _log_make_prefix(char *prefix, enum nugu_log_level level,
			    const char *filename, const char *funcname,
			    int line)
{
	const char *pretty_filename = NULL;
	int len = 0;

	if (_log_prefix_fields & NUGU_LOG_PREFIX_TIMESTAMP) {
		struct tm ti;
		int msec;
		gint64 microseconds;
		time_t now;

		microseconds = g_get_real_time();

		msec = (int)((microseconds % 1000000) / 1000);
		now = (time_t)(microseconds / 1000000);

#ifdef _WIN32
		localtime_s(&ti, &now);
#else
		localtime_r(&now, &ti);
#endif

		len += (int)strftime(prefix, 15, "%m-%d %H:%M:%S", &ti);
		len += snprintf(prefix + len, 6, ".%03d ", msec);
	}

	if (_log_prefix_fields & NUGU_LOG_PREFIX_PID ||
	    _log_prefix_fields & NUGU_LOG_PREFIX_TID) {
		pid_t pid = getpid();

		if (_log_prefix_fields & NUGU_LOG_PREFIX_PID)
			len += sprintf(prefix + len, "%d ", pid);

		if (_log_prefix_fields & NUGU_LOG_PREFIX_TID) {
			pid_t tid;
			int is_main_thread = 1;

#ifdef HAVE_SYSCALL
#ifdef __APPLE__
			pthread_threadid_np(NULL, (uint64_t *)&tid);
			is_main_thread = pthread_main_np();
#else
			tid = (pid_t)syscall(SYS_gettid);
			if (pid != 0 && pid != tid)
				is_main_thread = 0;
#endif
#elif defined(__MSYS__) || defined(_WIN32)
			tid = (pid_t)GetCurrentThreadId();
			if (pid != 0 && pid != tid)
				is_main_thread = 0;
#else
			tid = (pid_t)gettid();
			if (pid != 0 && pid != tid)
				is_main_thread = 0;
#endif

			if (is_main_thread == 0 && COLOR_THREAD_ID[0] != '\0')
				len += sprintf(prefix + len,
					       COLOR_THREAD_ID "%d " COLOR_OFF,
					       tid);
			else
				len += sprintf(prefix + len, "%d ", tid);
		}
	}

	if (_log_prefix_fields & NUGU_LOG_PREFIX_LEVEL) {
		prefix[len++] = _log_level_map[level].mark;
		prefix[len++] = ' ';
	}

	if ((_log_prefix_fields & NUGU_LOG_PREFIX_FILENAME) && filename) {
		pretty_filename = strrchr(filename, '/');
		if (!pretty_filename)
			pretty_filename = filename;
		else
			pretty_filename++;
	}

	if ((_log_prefix_fields & NUGU_LOG_PREFIX_FILEPATH) && filename)
		pretty_filename = filename;

	if (pretty_filename) {
		size_t field_len;

		field_len = strlen(pretty_filename);
		if (field_len > MAX_FIELDSIZE_FILENAME) {
			len += snprintf(prefix + len,
					MAX_FIELDSIZE_FILENAME + 5, "<~%s> ",
					pretty_filename + field_len -
						MAX_FIELDSIZE_FILENAME);
		} else
			len += snprintf(prefix + len, field_len + 5, "<%s> ",
					pretty_filename);

		/* Filename with line number */
		if ((_log_prefix_fields & NUGU_LOG_PREFIX_LINE) && line >= 0) {
			len--;
			len--;
			len += snprintf(prefix + len, 9, ":%d> ", line);
			*(prefix + len - 1) = ' ';
		}
	} else {
		/* Standalone line number */
		if ((_log_prefix_fields & NUGU_LOG_PREFIX_LINE) && line >= 0) {
			len += snprintf(prefix + len, 9, "<%d> ", line);
			*(prefix + len - 1) = ' ';
		}
	}

	if ((_log_prefix_fields & NUGU_LOG_PREFIX_FUNCTION) && funcname) {
		size_t field_len;

		field_len = strlen(funcname);
		if (field_len > MAX_FIELDSIZE_FUNCNAME) {
			len += snprintf(prefix + len,
					MAX_FIELDSIZE_FUNCNAME + 3, "~%s ",
					funcname + field_len -
						MAX_FIELDSIZE_FUNCNAME);
		} else
			len += snprintf(prefix + len, field_len + 2, "%s ",
					funcname);
	}

	/* Remove last space */
	if (len > 0) {
		if (*(prefix + len - 1) == ' ') {
			*(prefix + len - 1) = 0;
			len--;
		}
	}

	return len;
}

static void _log_formatted(enum nugu_log_module module,
			   enum nugu_log_level level, const char *filename,
			   const char *funcname, int line, const char *format,
			   va_list arg)
{
	char prefix[MAX_LOG_LENGTH] = { 0 };
	int len = 0;
	FILE *fp = NULL;

	if (_log_prefix_fields > NUGU_LOG_PREFIX_NONE)
		len = _log_make_prefix(prefix, level, filename, funcname, line);

	if (_log_system == NUGU_LOG_SYSTEM_STDERR)
		fp = stderr;
	else if (_log_system == NUGU_LOG_SYSTEM_STDOUT)
		fp = stdout;
	else if (_log_system == NUGU_LOG_SYSTEM_CUSTOM && _log_handler) {
		char msg[MAX_LOG_LENGTH];

		if (vsnprintf(msg, MAX_LOG_LENGTH, format, arg) >=
		    MAX_LOG_LENGTH)
			msg[MAX_LOG_LENGTH - 1] = 0;

		_log_handler(module, level, prefix, msg,
			     _log_handler_user_data);

		return;
	}

	if (fp == NULL)
		return;

	pthread_mutex_lock(&_log_mutex);

	/* NOLINTNEXTLINE(cert-err33-c) */
	if (len > 0)
		fprintf(fp, "%s ", prefix);

	if (_log_level_map[level].color != NULL) {
		/* NOLINTNEXTLINE(cert-err33-c) */
		fputs(_log_level_map[level].color, fp);

		/* NOLINTNEXTLINE(cert-err33-c) */
		vfprintf(fp, format, arg);

		/* NOLINTNEXTLINE(cert-err33-c) */
		fputs(COLOR_OFF "\n", fp);
	} else {
		/* NOLINTNEXTLINE(cert-err33-c) */
		vfprintf(fp, format, arg);

		/* NOLINTNEXTLINE(cert-err33-c) */
		fputc('\n', fp);
	}

	/* NOLINTNEXTLINE(cert-err33-c) */
	fflush(fp);

	pthread_mutex_unlock(&_log_mutex);
}

#ifndef _WIN32
static void _syslog_formatted(enum nugu_log_module module,
			      enum nugu_log_level level, const char *filename,
			      const char *funcname, int line,
			      const char *format, va_list arg)
{
	int len;
	char prefix[MAX_LOG_LENGTH] = { 0 };
	GString *buf;

	len = _log_make_prefix(prefix, level, filename, funcname, line);
	if (len > 0) {
		buf = g_string_new(prefix);
		g_string_append_c(buf, ' ');
	} else {
		buf = g_string_new(NULL);
	}

	if (_log_level_map[level].color != NULL) {
		g_string_append(buf, _log_level_map[level].color);
		g_string_append(buf, format);
		g_string_append(buf, COLOR_OFF);
	} else {
		g_string_append(buf, format);
	}

	vsyslog(_log_level_map[level].syslog_level, buf->str, arg);
	g_string_free(buf, TRUE);
}
#endif

void nugu_log_print(enum nugu_log_module module, enum nugu_log_level level,
		    const char *filename, const char *funcname, int line,
		    const char *format, ...)
{
	va_list arg;
	enum nugu_log_system log_system;

	if (!_log_override_checked) {
		pthread_mutex_lock(&_log_mutex);
		_log_check_override();
		pthread_mutex_unlock(&_log_mutex);
	}

	if (level > _log_level)
		return;

	/* The NUGU_LOG_LEVEL_ERROR always pass the module filtering */
	if (level > NUGU_LOG_LEVEL_ERROR) {
		if ((_log_module_bitset & module) == 0)
			return;
	}

	log_system = _log_system;

	switch (log_system) {
	case NUGU_LOG_SYSTEM_SYSLOG:
#ifndef _WIN32
		va_start(arg, format);
		if (_log_prefix_fields == NUGU_LOG_PREFIX_NONE)
			vsyslog(_log_level_map[level].syslog_level, format,
				arg);
		else
			_syslog_formatted(module, level, filename, funcname,
					  line, format, arg);
		va_end(arg);
		break;
#endif
	case NUGU_LOG_SYSTEM_STDERR:
	case NUGU_LOG_SYSTEM_STDOUT:
	case NUGU_LOG_SYSTEM_CUSTOM:
		va_start(arg, format);
		_log_formatted(module, level, filename, funcname, line, format,
			       arg);
		va_end(arg);
		break;

	case NUGU_LOG_SYSTEM_NONE:
	default:
		break;
	}
}

int nugu_log_set_system(enum nugu_log_system log_system)
{
	if (log_system > NUGU_LOG_SYSTEM_CUSTOM) {
		nugu_error("invalid log system(%d)", log_system);
		return -EINVAL;
	}

	pthread_mutex_lock(&_log_mutex);

	if (_log_override_enabled) {
		pthread_mutex_unlock(&_log_mutex);
		return 0;
	}

	_log_system = log_system;

	pthread_mutex_unlock(&_log_mutex);

	return 0;
}

enum nugu_log_system nugu_log_get_system(void)
{
	enum nugu_log_system log_system;

	if (!_log_override_checked) {
		pthread_mutex_lock(&_log_mutex);
		_log_check_override();
		pthread_mutex_unlock(&_log_mutex);
	}

	pthread_mutex_lock(&_log_mutex);
	log_system = _log_system;
	pthread_mutex_unlock(&_log_mutex);

	return log_system;
}

int nugu_log_set_handler(nugu_log_handler handler, void *user_data)
{
	if (!handler) {
		nugu_error("handler is NULL");
		return -EINVAL;
	}

	pthread_mutex_lock(&_log_mutex);

	if (_log_override_enabled) {
		pthread_mutex_unlock(&_log_mutex);
		return 0;
	}

	_log_system = NUGU_LOG_SYSTEM_CUSTOM;
	_log_handler = handler;
	_log_handler_user_data = user_data;

	pthread_mutex_unlock(&_log_mutex);

	return 0;
}

void nugu_log_set_prefix_fields(enum nugu_log_prefix field_set)
{
	pthread_mutex_lock(&_log_mutex);
	_log_prefix_fields = field_set;
	pthread_mutex_unlock(&_log_mutex);
}

enum nugu_log_prefix nugu_log_get_prefix_fields(void)
{
	enum nugu_log_prefix tmp;

	if (!_log_override_checked) {
		pthread_mutex_lock(&_log_mutex);
		_log_check_override();
		pthread_mutex_unlock(&_log_mutex);
	}

	pthread_mutex_lock(&_log_mutex);
	tmp = _log_prefix_fields;
	pthread_mutex_unlock(&_log_mutex);

	return tmp;
}

void nugu_log_set_modules(unsigned int bitset)
{
	pthread_mutex_lock(&_log_mutex);
	_log_module_bitset = bitset;
	pthread_mutex_unlock(&_log_mutex);
}

unsigned int nugu_log_get_modules(void)
{
	unsigned int tmp;

	if (!_log_override_checked) {
		pthread_mutex_lock(&_log_mutex);
		_log_check_override();
		pthread_mutex_unlock(&_log_mutex);
	}

	pthread_mutex_lock(&_log_mutex);
	tmp = _log_module_bitset;
	pthread_mutex_unlock(&_log_mutex);

	return tmp;
}

void nugu_log_set_level(enum nugu_log_level level)
{
	pthread_mutex_lock(&_log_mutex);
	_log_level = level;
	pthread_mutex_unlock(&_log_mutex);
}

enum nugu_log_level nugu_log_get_level(void)
{
	enum nugu_log_level tmp;

	if (!_log_override_checked) {
		pthread_mutex_lock(&_log_mutex);
		_log_check_override();
		pthread_mutex_unlock(&_log_mutex);
	}

	pthread_mutex_lock(&_log_mutex);
	tmp = _log_level;
	pthread_mutex_unlock(&_log_mutex);

	return tmp;
}

void nugu_log_set_protocol_line_limit(int length)
{
	pthread_mutex_lock(&_log_mutex);
	if (length <= 0)
		_log_line_limit = -1;
	else
		_log_line_limit = length;
	pthread_mutex_unlock(&_log_mutex);
}

int nugu_log_get_protocol_line_limit(void)
{
	int tmp;

	if (!_log_override_checked) {
		pthread_mutex_lock(&_log_mutex);
		_log_check_override();
		pthread_mutex_unlock(&_log_mutex);
	}

	pthread_mutex_lock(&_log_mutex);
	tmp = _log_line_limit;
	pthread_mutex_unlock(&_log_mutex);

	return tmp;
}

void nugu_hexdump(enum nugu_log_module module, const uint8_t *data,
		  size_t data_size, const char *header, const char *footer,
		  const char *lineindent)
{
	size_t i;
	size_t j;
	char linebuf[HEXDUMP_LINE_BUFSIZE];
	FILE *fp = NULL;

	g_return_if_fail(data != NULL);
	g_return_if_fail(data_size > 0);

	pthread_mutex_lock(&_log_mutex);

	if (!_log_override_checked)
		_log_check_override();

	if ((_log_module_bitset & module) == 0) {
		pthread_mutex_unlock(&_log_mutex);
		return;
	}

	if (_log_system == NUGU_LOG_SYSTEM_STDERR)
		fp = stderr;
	else if (_log_system == NUGU_LOG_SYSTEM_STDOUT)
		fp = stdout;
	else {
		pthread_mutex_unlock(&_log_mutex);
		return;
	}

	if (header)
		fprintf(fp, "%s", header);

	for (i = 0; i < data_size; i += HEXDUMP_COLUMN_SIZE) {
		int col = snprintf(linebuf, HEXDUMP_LINE_BUFSIZE,
				   "0x%04X: ", (unsigned int)i);

		/* hex */
		for (j = i; j < i + HEXDUMP_COLUMN_SIZE; j++) {
			if (j >= data_size) {
				/* fill empty space between hex and ascii */
				linebuf[col++] = ' ';
				linebuf[col++] = ' ';
				linebuf[col++] = ' ';
			} else {
				uint8_t x = (data[j] & 0xF0) >> 4;

				if (x < 10)
					linebuf[col++] = '0' + x;
				else
					linebuf[col++] = 'A' + (x - 10);

				x = (data[j] & 0x0F);
				if (x < 10)
					linebuf[col++] = '0' + x;
				else
					linebuf[col++] = 'A' + (x - 10);

				linebuf[col++] = ' ';
			}

			/* middle space */
			if ((j + 1) % (HEXDUMP_COLUMN_SIZE / 2) == 0) {
				linebuf[col++] = ' ';
				linebuf[col++] = ' ';
			}
		}

		/* ascii */
		for (j = i; j < i + HEXDUMP_COLUMN_SIZE; j++) {
			if (j >= data_size)
				linebuf[col++] = ' ';
			else if (isprint(data[j]))
				linebuf[col++] = data[j];
			else
				linebuf[col++] = '.';
		}

		linebuf[col++] = '\n';
		linebuf[col++] = '\0';

		if (lineindent)
			fprintf(fp, "%s%s", lineindent, linebuf);
		else
			fprintf(fp, "%s", linebuf);
	}

	if (i % HEXDUMP_COLUMN_SIZE != 0)
		/* NOLINTNEXTLINE(cert-err33-c) */
		fputc('\n', fp);

	if (footer)
		fprintf(fp, "%s", footer);

	pthread_mutex_unlock(&_log_mutex);
}
