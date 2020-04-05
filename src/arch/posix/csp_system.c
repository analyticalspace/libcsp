/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 Gomspace ApS (http://www.gomspace.com)
Copyright (C) 2012 AAUSAT3 Project (http://aausat3.space.aau.dk)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <sys/reboot.h>
#include <linux/reboot.h>

#include <csp/arch/posix/csp_system.h>
#include <csp/csp_debug.h>

#ifdef CSP_HAVE_LIBPROCPS
#include <proc/readproc.h>

void cpu_percent_calc(proc_t * in)
{
	unsigned long long used_jiffies;
	unsigned long pcpu = 0;
	unsigned long long seconds;
	struct sysinfo sysinf;
	long sys_hertz = sysconf(_SC_CLK_TCK);

	/* this should probably never fail given
	 * our usage. */
	if (0 != sysinfo(&sysinf)) { return; }

	used_jiffies = in->utime + in->stime;
	used_jiffies += (in->cutime + in->cstime);

	seconds = sysinf.uptime - in->start_time / sys_hertz;

	if(seconds)
		pcpu = (used_jiffies * 1000ULL / sys_hertz) / seconds;

	in->pcpu = pcpu;
}

int csp_sys_tasklist(char * out, size_t out_size)
{
	size_t offset = 0;
	const size_t offset_max = out_size;

#define SAFE_SNPRINTF(f, ...)								   \
	do {													   \
		size_t n = offset_max - offset;						   \
		int ret = snprintf(out + offset, n, f, ##__VA_ARGS__); \
															   \
		if (ret == -1)										   \
			goto insert_error;								   \
															   \
		offset += ret;										   \
	} while (0)

	pid_t self[1] = { getpid() };
	PROCTAB * pt = NULL;
	proc_t * proc = NULL;
	proc_t * task = NULL; /* ie - thread */

	if (NULL ==
		(pt = openproc(PROC_PID | PROC_FILLSTAT, &self)))
	{
		goto tasklist_error;
	}

	if (NULL ==
		(proc = readproc(pt, NULL)))
	{
		goto tasklist_error;
	}

	/* insert top level process */
	cpu_percent_calc(proc);
	SAFE_SNPRINTF("%-10s %-8d %.2f%%", proc->cmd,
				  self[0], proc->pcpu / 10.f);

	while ((task = readtask(pt, proc, NULL)))
	{
		if (offset >= offset_max) {
			break;
		}

		/* skip main thread as those stats are included
		 * in the process top-level stats */
		if (task->tid != proc->tid)
		{
			cpu_percent_calc(task);
			SAFE_SNPRINTF("\r\n%-10s %-8d %.2f%%", task->cmd,
						  task->tid, task->pcpu / 10.f);
		}

		freeproc(task);
		task = NULL;
	}

	if (task) freeproc(task);
	if (proc) freeproc(proc);
	if (pt)   closeproc(pt);

	return CSP_ERR_NONE;

insert_error:
tasklist_error:
	if (task) freeproc(task);
	if (proc) freeproc(proc);
	if (pt)   closeproc(pt);

	out[0] = '\0';
	return CSP_ERR_INVAL;
}

#else /* !defined(CSP_HAVE_LIBPROCPS) */

static const char no_tasklist_msg[] = "Tasklist not available on POSIX\0";

int csp_sys_tasklist(char * out, size_t out_size) {
	// TODO check static string against out_size
	strcpy(out, no_tasklist_msg);
	return CSP_ERR_NONE;
}
#endif /* defined(CSP_HAVE_LIBPROCPS) */

uint32_t csp_sys_memfree(void) {

	uint32_t total = 0;
	struct sysinfo info;

	(void) sysinfo(&info);
	total = info.freeram * info.mem_unit;

	return total;
}

// helper for doing log and mapping result to CSP_ERR
static int csp_sys_log_and_return(const char * function, int res) {

	if (res != 0) {
		csp_log_warn("%s: failed to execute, returned error: %d, errno: %d", function, res, errno);
		return CSP_ERR_INVAL; // no real suitable error code
	}

	csp_log_info("%s: executed", function);
	return CSP_ERR_NONE;
}

int csp_sys_reboot_using_system(void) {
	return csp_sys_log_and_return(__FUNCTION__, system("reboot"));
}

int csp_sys_reboot_using_reboot(void) {
	sync(); // Sync filesystem
	return csp_sys_log_and_return(__FUNCTION__, reboot(LINUX_REBOOT_CMD_RESTART));
}

int csp_sys_shutdown_using_system(void) {
	return csp_sys_log_and_return(__FUNCTION__, system("halt"));
}

int csp_sys_shutdown_using_reboot(void) {
	sync(); // Sync filesystem
	return csp_sys_log_and_return(__FUNCTION__, reboot(LINUX_REBOOT_CMD_HALT));
}

void csp_sys_set_color(csp_color_t color) {

	unsigned int color_code, modifier_code;
	switch (color & COLOR_MASK_COLOR) {
		case COLOR_BLACK:
			color_code = 30; break;
		case COLOR_RED:
			color_code = 31; break;
		case COLOR_GREEN:
			color_code = 32; break;
		case COLOR_YELLOW:
			color_code = 33; break;
		case COLOR_BLUE:
			color_code = 34; break;
		case COLOR_MAGENTA:
			color_code = 35; break;
		case COLOR_CYAN:
			color_code = 36; break;
		case COLOR_WHITE:
			color_code = 37; break;
		case COLOR_RESET:
		default:
			color_code = 0; break;
	}

	switch (color & COLOR_MASK_MODIFIER) {
		case COLOR_BOLD:
			modifier_code = 1; break;
		case COLOR_UNDERLINE:
			modifier_code = 2; break;
		case COLOR_BLINK:
			modifier_code = 3; break;
		case COLOR_HIDE:
			modifier_code = 4; break;
		case COLOR_NORMAL:
		default:
			modifier_code = 0; break;
	}

	printf("\033[%u;%um", modifier_code, color_code);
}
