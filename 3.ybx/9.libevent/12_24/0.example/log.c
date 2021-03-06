#include <event3/event-config.h>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "event3/event.h"
#include "event3/util.h"
#include "log-internal.h"

static void _warn_helper(int severity, const char *errstr,
						 const char *fmt, va_list ap);
static void event_log(int severity, const char *msg);
static void event_exit(int errcode) EV_NORETURN;

//自定义fatal callback
static event_fatal_cb fatal_fn = NULL;

//设置fatal_fn
void
event_set_fatal_callback(event_fatal_cb cb)
{
    fatal_fn = cb;
}

static void
event_exit(int errcode)
{
    if (fatal_fn) {
        fatal_fn(errcode);
        exit(errcode); /* should never be reached */
    } else if (errcode == _EVENT_ERR_ABORT)
        abort();
    else
        exit(errcode);
}

void
event_err(int eval, const char *fmt, ...)
{
    va_list ap; 
    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_ERR, strerror(errno), fmt, ap);
    va_end(ap);
    event_exit(eval);
}

void
event_warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_WARN, strerror(errno), fmt, ap);
    va_end(ap);
}

//eval:是exit(eval)中使用的
void
event_sock_err(int eval, evutil_socket_t sock, const char *fmt, ...)
{
    va_list ap;
    int err = evutil_socket_geterror(sock);
    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_ERR, evutil_socket_error_to_string(err), fmt, ap);
    va_end(ap);
    event_exit(eval);
}

void
event_sock_warn(evutil_socket_t sock, const char *fmt, ...)
{
    va_list ap;
    int err = evutil_socket_geterror(sock);
    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_WARN, evutil_socket_error_to_string(err), fmt, ap);
    va_end(ap);
}

void
event_errx(int eval, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_ERR, NULL, fmt, ap);
    va_end(ap);
    event_exit(eval);
}

void
event_warnx(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_WARN, NULL, fmt, ap);
    va_end(ap);
}

void
event_msgx(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_MSG, NULL, fmt, ap);
    va_end(ap);
}

void
_event_debugx(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _warn_helper(_EVENT_LOG_DEBUG, NULL, fmt, ap);
    va_end(ap);
}

//组织日志buf传给event_log
static void
_warn_helper(int severity, const char *errstr, const char *fmt, va_list ap)
{
	char buf[1024];
	size_t len;

	//如果有可变参数，就把可变参数格式化到一个缓存区buf中。
	if (fmt != NULL)
		evutil_vsnprintf(buf, sizeof(buf), fmt, ap);
	else
		buf[0] = '\0';

	//如果有额外的信息描述，把这些信息追加到可变参数的后面。
	if (errstr) {
		len = strlen(buf);
	   //-3是因为还有另外三个字符，冒号、空格和\0。
		if (len < sizeof(buf) - 3) {
			evutil_snprintf(buf + len, sizeof(buf) - len, ": %s", errstr);
		}
	}
	
	//把缓存区的数据作为一条日志记录，调用Libevent的日志函数。
	event_log(severity, buf);//这个函数也是最前面的说到的那个日志处理函数
}



//自定义日志函数
static event_log_cb log_fn = NULL;

//指定全局日志函数
void
event_set_log_callback(event_log_cb cb)
{
    log_fn = cb;
}

static void
event_log(int severity, const char *msg)
{
	//指定了自定义日志函数
    if (log_fn)
        log_fn(severity, msg);
    else {
        const char *severity_str;
		switch (severity) {
			case _EVENT_LOG_DEBUG:
				severity_str = "debug";
				break;
			case _EVENT_LOG_MSG:
				severity_str = "msg";
				break;
			case _EVENT_LOG_WARN:
				severity_str = "warn";
				break;
			case _EVENT_LOG_ERR:
				severity_str = "err";
				break;
			default:
				severity_str = "???";
				break;
		}
		(void)fprintf(stderr, "[%s] %s\n", severity_str, msg);
	}
}
