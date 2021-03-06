#include "fmacros.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "anet.h"

static void anetSetError(char *err, const char *fmt, ...)
{
	va_list ap;
	if (!err) return;
	va_start(ap, fmt);
	vsnprintf(err, ANET_ERR_LEN, fmt, ap);
	va_end(ap);
}

int anetSetBlock(char *err, int fd, int non_block) {
	int flags;

	/* Set the socket blocking (if non_block is zero) or non-blocking.
	 * Note that fcntl(2) for F_GETFL and F_SETFL can't be
	 * interrupted by a signal. */
	if ((flags = fcntl(fd, F_GETFL)) == -1) {
		anetSetError(err, "fcntl(F_GETFL): %s", strerror(errno));
		return ANET_ERR;
	}

	if (non_block)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if (fcntl(fd, F_SETFL, flags) == -1) {
		anetSetError(err, "fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno));
		return ANET_ERR;
	}
	return ANET_OK;
}

int anetNonBlock(char *err, int fd) {
	return anetSetBlock(err,fd,1);
}

int anetBlock(char *err, int fd) {
	return anetSetBlock(err,fd,0);
}

/* Set TCP keep alive option to detect dead peers. The interval option
 * is only used for Linux as we are using Linux-specific APIs to set
 * the probe send time, interval, and count. */
int anetKeepAlive(char *err, int fd, int interval)
{
	int val = 1;

	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) == -1)
	{
		anetSetError(err, "setsockopt SO_KEEPALIVE: %s", strerror(errno));
		return ANET_ERR;
	}

#ifdef __linux__
	/* Default settings are more or less garbage, with the keepalive time
	 * set to 7200 by default on Linux. Modify settings to make the feature
	 * actually useful. */

	/* Send first probe after interval. */
	val = interval;
	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val)) < 0) {
		anetSetError(err, "setsockopt TCP_KEEPIDLE: %s\n", strerror(errno));
		return ANET_ERR;
	}

	/* Send next probes after the specified interval. Note that we set the
	 * delay as interval / 3, as we send three probes before detecting
	 * an error (see the next setsockopt call). */
	val = interval/3;
	if (val == 0) val = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val)) < 0) {
		anetSetError(err, "setsockopt TCP_KEEPINTVL: %s\n", strerror(errno));
		return ANET_ERR;
	}

	/* Consider the socket in error state after three we send three ACK
	 * probes without getting a reply. */
	val = 3;
	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val)) < 0) {
		anetSetError(err, "setsockopt TCP_KEEPCNT: %s\n", strerror(errno));
		return ANET_ERR;
	}
#else
	((void) interval); /* Avoid unused var warning for non Linux systems. */
#endif

	return ANET_OK;
}

static int anetSetTcpNoDelay(char *err, int fd, int val)
{
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) == -1)
	{
		anetSetError(err, "setsockopt TCP_NODELAY: %s", strerror(errno));
		return ANET_ERR;
	}
	return ANET_OK;
}

int anetEnableTcpNoDelay(char *err, int fd)
{
	return anetSetTcpNoDelay(err, fd, 1);
}

int anetDisableTcpNoDelay(char *err, int fd)
{
	return anetSetTcpNoDelay(err, fd, 0);
}

static int anetSetReuseAddr(char *err, int fd) {
	int yes = 1;
	/* Make sure connection-intensive things like the redis benckmark
	 * will be able to close/open sockets a zillion of times */
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		anetSetError(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
		return ANET_ERR;
	}
	return ANET_OK;
}

static int anetListen(char *err, int s, struct sockaddr *sa, socklen_t len, int backlog) {
	if (bind(s,sa,len) == -1) {
		anetSetError(err, "bind: %s", strerror(errno));
		close(s);
		return ANET_ERR;
	}

	if (listen(s, backlog) == -1) {
		anetSetError(err, "listen: %s", strerror(errno));
		close(s);
		return ANET_ERR;
	}
	return ANET_OK;
}

static int anetV6Only(char *err, int s) {
	int yes = 1;
	if (setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,&yes,sizeof(yes)) == -1) {
		anetSetError(err, "setsockopt: %s", strerror(errno));
		close(s);
		return ANET_ERR;
	}
	return ANET_OK;
}

static int _anetTcpServer(char *err, int port, char *bindaddr, int af, int backlog)
{
	int s = -1, rv;
	char _port[6];  /* strlen("65535") */
	struct addrinfo hints, *servinfo, *p;

	snprintf(_port,6,"%d",port);
	memset(&hints,0,sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;    /* No effect if bindaddr != NULL */

	if ((rv = getaddrinfo(bindaddr,_port,&hints,&servinfo)) != 0) {
		anetSetError(err, "%s", gai_strerror(rv));
		return ANET_ERR;
	}
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
			continue;

		if (af == AF_INET6 && anetV6Only(err,s) == ANET_ERR) goto error;
		if (anetSetReuseAddr(err,s) == ANET_ERR) goto error;
		if (anetListen(err,s,p->ai_addr,p->ai_addrlen,backlog) == ANET_ERR) goto error;
		goto end;
	}
	if (p == NULL) {
		anetSetError(err, "unable to bind socket, errno: %d", errno);
		goto error;
	}

error:
	if (s != -1) close(s);
	s = ANET_ERR;
end:
	freeaddrinfo(servinfo);
	return s;
}//end of _anetTcpServer()




int anetTcpServer(char *err, int port, char *bindaddr, int backlog)
{
	return _anetTcpServer(err, port, bindaddr, AF_INET, backlog);
}

int anetTcp6Server(char *err, int port, char *bindaddr, int backlog)
{
	return _anetTcpServer(err, port, bindaddr, AF_INET6, backlog);
}


static int anetGenericAccept(char *err, int s, struct sockaddr *sa, socklen_t *len) {
	int fd;
	while(1) {
		fd = accept(s,sa,len);
		if (fd == -1) {
			if (errno == EINTR)
				continue;
			else {
				anetSetError(err, "accept: %s", strerror(errno));
				return ANET_ERR;
			}
		}
		break;
	}
	return fd;
}

int anetTcpAccept(char *err, int s, char *ip, size_t ip_len, int *port) {
	int fd;
	struct sockaddr_storage sa;
	socklen_t salen = sizeof(sa);
	if ((fd = anetGenericAccept(err,s,(struct sockaddr*)&sa,&salen)) == -1)
		return ANET_ERR;

	if (sa.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&sa;
		if (ip) inet_ntop(AF_INET,(void*)&(s->sin_addr),ip,ip_len);
		if (port) *port = ntohs(s->sin_port);
	} else {
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
		if (ip) inet_ntop(AF_INET6,(void*)&(s->sin6_addr),ip,ip_len);
		if (port) *port = ntohs(s->sin6_port);
	}
	return fd;
}

int anetPeerToString(int fd, char *ip, size_t ip_len, int *port) {
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);

    if (getpeername(fd,(struct sockaddr*)&sa,&salen) == -1) goto error;
    if (ip_len == 0) goto error;

    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sa;
        if (ip) inet_ntop(AF_INET,(void*)&(s->sin_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin_port);
    } else if (sa.ss_family == AF_INET6) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6,(void*)&(s->sin6_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin6_port);
    } else if (sa.ss_family == AF_UNIX) {
        if (ip) strncpy(ip,"/unixsocket",ip_len);
        if (port) *port = 0;
    } else {
        goto error;
    }
    return 0;

error:
    if (ip) {
        if (ip_len >= 2) {
            ip[0] = '?';
            ip[1] = '\0';
        } else if (ip_len == 1) {
            ip[0] = '\0';
        }
    }
    if (port) *port = 0;
    return -1;
}

int anetFormatAddr(char *buf, size_t buf_len, char *ip, int port) {
    return snprintf(buf,buf_len, strchr(ip,':') ?
           "[%s]:%d" : "%s:%d", ip, port);
}

/* Like anetFormatAddr() but extract ip and port from the socket's peer. */
int anetFormatPeer(int fd, char *buf, size_t buf_len) {
    char ip[INET6_ADDRSTRLEN];
    int port;

    anetPeerToString(fd,ip,sizeof(ip),&port);
    return anetFormatAddr(buf, buf_len, ip, port);
}
