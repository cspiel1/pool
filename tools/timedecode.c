#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>


static int body_value(char *val, size_t vlen, const char *body, const char *key)
{
    size_t klen = strlen(key);
    const char *p;
    const char *e;
    size_t l = vlen;

    p = strstr(body, key);
    if (!p)
        return ENODATA;

    p += klen + 1;
    e  = strchr(p, '&');
    if (!e)
        e = p + strlen(p);

    if (e <= p)
        return ENODATA;

    if (e - p < l)
	l = e - p;

    strncpy(val, p, l);
    return 0;
}


static time_t convert_time(const char *stime)
{
    struct tm tm;

    if (!stime)
	return EINVAL;

    memset(&tm, 0, sizeof tm);
    if (sscanf(stime, "%d%%3A%d", &tm.tm_hour, &tm.tm_min) <= 0)
	return false;

    return mktime(&tm);
}


static time_t current_time(void)
{
    struct tm tmc, tmcc;
    struct timespec tp;

    memset(&tmcc, 0, sizeof tmcc);

    clock_gettime(CLOCK_REALTIME, &tp);
    localtime_r(&tp.tv_sec, &tmc);
    tmcc.tm_hour = tmc.tm_hour;
    tmcc.tm_min = tmc.tm_min;
    return mktime(&tmcc);
}


bool check_time(time_t times, int duration)
{
    time_t timec;

    timec = current_time();
    return times <= timec && timec <= times + 3600 * duration;
}


void run_it(const char *buf)
{
    char stime[10];
    char dur[10];
    char ctime[10];
    time_t times, timec;
    int duration;
    int err;

    err  = body_value(stime, sizeof stime, buf, "stime");
    err |= body_value(dur, sizeof dur, buf, "duration");

    if (err) {
	printf("Error\n");
	return;
    }

    times = convert_time(stime);
    duration = atoi(dur);
    timec = current_time();

    strftime(stime, sizeof stime, "%H:%M", localtime(&times));
    printf("Duration %d Time %s %s.\n", duration, stime, check_time(times, duration) ? "YES" : "NO");
    strftime(ctime, sizeof ctime, "%H:%M", localtime(&timec));
    printf("Current: %s\n", ctime);
}


int main(int argc, char *argv[])
{
	const char *buf;

	if (argc < 2)
		return 1;

	buf = argv[1];
	run_it(buf);
	return 0;
}


