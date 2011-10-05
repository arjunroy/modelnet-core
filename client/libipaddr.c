/*
 * modelnet  libipaddr.c
 *
 *     An LD_PRELOAD shared library to transparently set source and
 *     destitation IP addresses used by applications.
 *
 * author Kevin Walsh
 *
Copyright (c) 2003, Duke University  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

* Neither the name of the University nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

/*
 * Modelnet IP address control
 *
 * This binary allows modelnet apps to transparently
 * set the source and destination IP address.
 *
 * Our goals:
 *  - set the default source ip address (using env var SRCIP)
 *  - force all packets to go out to forwarder (disable by setting KEEP_DSTIP)
 *
 *  - set the local host name (using env var MN_LOCALHOST).  not required.
 *  - emulate a private dns (using env var HOSTS).    not required.
 *
 * We need to allow some traffic to external network services, like yp,
 * dns, etc., or Java, chord, ps, ls -l, and other programs will not work.
 * In these cases, the source ip address must be the real address.
 * 
 * It works like this:
 * 1 - Connection-oriented listeners must call bind(). We assume that
 *   these listeners are only listening for modelnet traffic. So
 *   during the bind() we go ahead and assume that the local address
 *   should be 10.x.x.x (tentatively).
 * 2 - Connection-oriented senders might call bind(), but will always
 *   call connect(). The bind has already assumed a 10.x.x.x local
 *   address. If the connect detirmines that the assumption was wrong,
 *   then we try must try to rebind to the regular host address.
 * 3 - Connectionless recievers must call bind() before
 *   calling recv/recvfrom/recvmsg. In this case, we assume that
 *   they are only listening for modelnet traffic, and that the
 *   bind assumption of a 10.x.x.x local address is okay.
 * 4 - Connectionless senders may skip the bind step, and just call
 *   sendto/sendmsg. We check the destination address, and if
 *   the bind() assumption was wrong, we try to fix it here.
 * 5 - Connectionless senders may want to call send/write, in which
 *   case they must do a connect() first. This case is already
 *   covered in the same way as the connection-oriented connect() case.
 *
 * To do all this, we keep around 2 state bits for each socket, saying
 * if the socket is bound to 10.x.x.x (M) or the regular host addr (S) or
 * nothing (U).
 * 1 - bind() always uses 10.x.x.x as the local addr and sets M.
 * 2 - connect() distinguishes between modelnet and external destinations.
 *   If modelnet dest and U, then bind to 10.x.x.x
 *   If modelnet dest and S, then try to unbind and rebind to 10.x.x.x
 *   If external dest and U, then bind to the regular address
 *   If modelnet dest and M, then try to unbind and rebind to the regular addr
 * 3 - recv()/recvfrom()/recvmsg() don't do anything
 * 4 - sendto()/sendmsg() do same thing as connect() did
 * 5 - send() doesn't do anything.
 *
 * When we unbind and rebind, we may not be able to get the same port
 * number again. We just have to live with that possibility.
 */


#define _GNU_SOURCE  /* to enable gcc magic in dlfcn.h for shared libs */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#ifdef linux
#include <linux/limits.h>
#define RET_TYPE ssize_t
#else
#ifdef __FreeBSD__
#include <sys/syslimits.h>
#endif
#define RET_TYPE int
#endif

#define DDB(x) do { if (mn_debug > 0) { x } } while (0);
#define DEB(x) do { if (mn_debug > 1) { x } } while (0);

/* original system routines */

/* sockets */

static int (*f_socket)(int, int, int);
static int (*f_socketpair)(int, int, int, int[2]);
static int (*f_bind)(int, const struct sockaddr *, socklen_t);
static int (*f_getsockname)(int, struct sockaddr *, socklen_t *);
static int (*f_connect)(int, const struct sockaddr *, socklen_t);
static int (*f_getpeername)(int, struct sockaddr *, socklen_t *);
static int (*f_send)(int, const void *, size_t, int);
static int (*f_recv)(int, void *, size_t, int);
static int (*f_sendto)(int, const void *, size_t, int,
			const struct sockaddr *, socklen_t);
static int (*f_recvfrom)(int, void *, size_t, int,
			struct sockaddr *, socklen_t *);
static int (*f_sendmsg)(int, const struct msghdr *, int);
static int (*f_recvmsg)(int, struct msghdr *, int);
static int (*f_getsockopt)(int, int, int, void *, socklen_t *);
static int (*f_setsockopt)(int, int, int, const void *, socklen_t);
static int (*f_listen)(int, int);
static int (*f_accept)(int, struct sockaddr *, socklen_t *);
static int (*f_shutdown)(int, int);

/* legacy nameserver */

static struct hostent *(*f_gethostbyname)(const char *);
static struct hostent *(*f_gethostbyname2)(const char *, int);
#ifdef __GLIBC__
static struct hostent *(*f_gethostbyaddr)(const void *, socklen_t, int);
static int (*f_gethostname)(char *name, size_t namelen);
#else
#ifdef __FreeBSD__
static struct hostent  *(*f_gethostbyaddr) __P((const char *, int, int));
static int (*f_gethostname) __P((char *, int));
#endif /*  __FreeBSD__ */
#endif /*  __GLIBC__ */
/* static struct hostent *(*f_gethostent)(void);	*/
/* static void (*f_sethostent)(int stayopen);		*/
/* static void (*f_endhostent)(void);			*/

/* new nameserver */
/* these don't seem to be implemented anywhere?!?! */
/*
static struct hostent *(*f_getipnodebyname)(const char *, int, int, int *);
static struct hostent *(*f_getipnodebyaddr)(const void *, size_t, int, int *);
static void (*f_freehostent)(struct hostent *ip);
static int (*f_getaddrinfo)(const char *, const char *,
                       const struct addrinfo *, struct addrinfo **);
static void (*f_freeaddrinfo)(struct addrinfo *);
*/

/* misc */


/*
 * Other candidates:
 * uname(2), getdomainname(2)
 * resolver(3), resolver(5), resolv+(8), named(8)
 */

/* modelnet variables */

static unsigned long mn_localip;
static char *mn_localhost;
static char *hostsfile=NULL;
static int mn_debug;
static int mn_keep_dstip;
static char localvname[256];

/* NOTE: the use of fd_set limits the number of sockets to FD_SETSIZE-1 */
fd_set mn_unbound_sockets;	/* not yet bound */
fd_set mn_modelnet_sockets;	/* bound to 10.x.x.x */
fd_set mn_hasport_sockets;	/* bound to a port (e.g. not "any") */

#define SOCK_UNBOUND(s) (FD_ISSET((s), &mn_unbound_sockets))
#define SOCK_MNET(s)	(FD_ISSET((s), &mn_modelnet_sockets))
#define SOCK_SYS(s)	(!SOCK_UNBOUND(s) && !SOCK_MNET(s))
#define SOCK_HASPORT(s)	(FD_ISSET((s), &mn_hasport_sockets))

/* some helpful routines */

#define MAKE_IP(a,b,c,d) \
    (((a)&0xff)<<24)|(((b)&0xff)<<16)|(((c)&0xff)<<8)|((d)&0xff)
#define IP_ADDR_QUAD_N(x) IP_ADDR_QUAD_H(ntohl(x))
#define IP_ADDR_QUAD_H(x) (unsigned long int)(((x)>>24)&0xff), \
                            (unsigned long int)(((x)>>16)&0xff), \
                            (unsigned long int)(((x)>>8)&0xff), \
                            (unsigned long int)((x)&0xff)
#define S_ADDR_QUAD_PORT(sa) \
	    IP_ADDR_QUAD_N((sa)->sin_addr.s_addr), (int)ntohs((sa)->sin_port)

#define IS_INET_ADDR(sock_addr,len) \
    ((sock_addr) != NULL && \
     (len) >= sizeof(struct sockaddr_in) && \
     (sock_addr)->sa_family == AF_INET)

#define IS_MNET_IP(ip) (((ip) & htonl(0xff000000)) == htonl(MAKE_IP(10,0,0,0)))

#define IS_MNET_ADDR(sin) (IS_MNET_IP((sin)->sin_addr.s_addr))

#define MNET_MAGIC_BIT (htonl(0x00800000))

/* we can't use inet_addr/inet_aton because they are part of sockets */
static unsigned long dots_to_ip(const char *s)
{
    unsigned int a,b,c,d;
    char *rest = NULL;

    if (!s) return 0;

    a = strtoul(s, &rest, 10);
    if (rest == s || rest[0] != '.') return 0;
    b = strtoul(s = rest+1, &rest, 10);
    if (rest == s || !rest || rest[0] != '.') return 0;
    c = strtoul(s = rest+1, &rest, 10);
    if (rest == s || rest[0] != '.') return 0;
    d = strtoul(s = rest+1, &rest, 10);
    if (rest == s) return 0;
    return ((a&0xff)<<24)|((b&0xff)<<16)|((c&0xff)<<8)|(d&0xff);
}

static void init_hostname_cache(void);
static int find_ip(unsigned long ip, char *vname, char *mname);

/* initialize library */

static void __attribute__ ((constructor))
init(void)
{
    const char *s;

    s = getenv("MN_DEBUG");
    /* fprintf(stderr, "mn: initializing libipaddr (MN_DEBUG='%s')\n", s); */
    if (s) mn_debug = atoi(s);

    DDB( fprintf(stderr, "mn: modelnet libipaddr\n"
	    "mn: compiled on: %s, %s\n", __DATE__, __TIME__); )

    DEB( fprintf(stderr, "mn: loading system socket routines\n"); )

#define GET_SYM(sym)							\
    f_##sym = dlsym(RTLD_NEXT, #sym);					\
    if (f_##sym == NULL) {						\
	DDB( fprintf(stderr, "mn:"				\
		"dlsym(%s) failed: %s\n", #sym, dlerror()); )		\
    }

    GET_SYM(socket);
    GET_SYM(socketpair);
    GET_SYM(bind);
    GET_SYM(getsockname);
    GET_SYM(connect);
    GET_SYM(getpeername);
    GET_SYM(send);
    GET_SYM(recv);
    GET_SYM(sendto);
    GET_SYM(recvfrom);
    GET_SYM(sendmsg);
    GET_SYM(recvmsg);
    GET_SYM(getsockopt);
    GET_SYM(setsockopt);
    GET_SYM(listen);
    GET_SYM(accept);
    GET_SYM(shutdown);

    GET_SYM(gethostbyname);
    GET_SYM(gethostbyname2);
    GET_SYM(gethostbyaddr);

/*
    GET_SYM(getipnodebyname);
    GET_SYM(getipnodebyaddr);
    GET_SYM(freehostent);
    GET_SYM(getaddrinfo);
    GET_SYM(freeaddrinfo);
*/

    GET_SYM(gethostname);

#undef GET_SYM

    /* get the modelnet environment variables */

    mn_localip = htonl(dots_to_ip(getenv("SRCIP")));
    mn_localhost = getenv("MN_LOCALHOST");
    hostsfile = getenv("HOSTS");
    if (!hostsfile && getenv("MN_MDB")) {
	hostsfile = malloc(PATH_MAX);
	sprintf(hostsfile,"%s/hosts",getenv("MN_MDB"));
	}
    mn_keep_dstip = getenv("KEEP_DSTIP")?1:0;

    init_hostname_cache();

    if (!mn_localhost && mn_localip && hostsfile) {
        char mname[256];
	find_ip(mn_localip, localvname, mname);
	mn_localhost = localvname;
	}
	    
    DDB (
	if (mn_localip)
	    fprintf(stderr, "mn: using local ip: %lu.%lu.%lu.%lu\n",
		    IP_ADDR_QUAD_N(mn_localip));    
	if (mn_localhost)
	    fprintf(stderr, "mn: using local host: %s\n", mn_localhost);
	if (hostsfile)
	    fprintf(stderr, "mn: using mdb: %s\n", hostsfile);
	if (mn_keep_dstip)
	    fprintf(stderr, "mn: keep dst IP as set by app. Do not force to forwarder\n");
	else
	    fprintf(stderr, "mn: force packets to forwarder (default)\n");

    )

    /* init rest of library */

    FD_ZERO(&mn_unbound_sockets);
    FD_ZERO(&mn_modelnet_sockets);
    FD_ZERO(&mn_hasport_sockets);

    DDB ( fprintf(stderr, "mn: exit init\n"); )
}


/* helper to do implicit binding of source address  */
static int bind_to(int s, unsigned long addr, int port_n)
{
    struct sockaddr_in sa;
    int ret;

    DDB( fprintf(stderr, "mn: bind_to(%d, %lu.%lu.%lu.%lu, %d)\n", s,
	    IP_ADDR_QUAD_N(addr), ntohs(port_n)); )

    /* memset(sa.sin_zero, 0, sizeof (sa.sin_zero)); */
    sa.sin_family = AF_INET;
    sa.sin_port = port_n;
    sa.sin_addr.s_addr = addr;
    ret = f_bind(s, (struct sockaddr *)&sa, sizeof(sa));
    if (ret == 0) {
	/* good, update state for socket */
	FD_CLR(s, &mn_unbound_sockets);
	if (IS_MNET_IP(addr))
	    FD_SET(s, &mn_modelnet_sockets);
	else
	    FD_CLR(s, &mn_modelnet_sockets);
	if (port_n != 0)
	    FD_SET(s, &mn_hasport_sockets);
	else
	    FD_CLR(s, &mn_hasport_sockets);
	DEB( fprintf(stderr, "mn: bind_to: socket %d bound to %s, %s port\n",
		s, IS_MNET_IP(addr)?"modelnet":"external",
		port_n?"chosen":"any"); )
    } else {
	DDB( fprintf(stderr, "mn: bind_to(%d) failed\n", s); )
    }
    return ret;
}

/* helper to unbind and rebind a socket */
static int rebind_to(int s, unsigned long addr)
{
    struct sockaddr_in old_sa;
    int s_new, len, sock_type;
    int err;

    DDB( fprintf(stderr, "mn: rebind_to(%d, %lu.%lu.%lu.%lu)\n", s,
	    IP_ADDR_QUAD_N(addr)); )

    len = sizeof(old_sa);
    if (f_getsockname(s, (struct sockaddr *)&old_sa, &len)) {
	fprintf(stderr, "mn: rebind_to(%d) failed: getsockname()\n", s);
	return -1;
    }
    len = sizeof(sock_type);
    if (f_getsockopt(s, SOL_SOCKET, SO_TYPE, &sock_type, &len)) {
	fprintf(stderr, "mn: rebind_to(%d) failed: getsockopt()\n", s);
	return -1;
    }
    s_new = f_socket(PF_INET, sock_type, 0);
    if (s_new < 0) {
	fprintf(stderr, "mn: rebind_to(%d) failed: socket()\n", s);
	return -1;
    }
    if (dup2(s_new, s) == -1) {
	fprintf(stderr, "mn: rebind_to(%d) failed: dup2(%d, %d)\n",s,s_new,s);
	close(s_new);
	return -1;
    }
    close(s_new);
    err = bind_to(s, addr, old_sa.sin_port);
    if (err && !SOCK_HASPORT(s)) {
	/* socket was originally bound to port 0 ("any") */
	/* but we can't get same port number... maybe the app doesn't care */
	fprintf(stderr,
		"mn: rebind_to(%d) trouble: Application originally requested\n"
		"    a socket on any port, and was assigned port %d as a\n"
		"    \"modelnet\" socket. It is now clear that a \"system\"\n"
		"    socket is needed instead, but the same port is no\n"
		"    longer available. Requesting any available port, and\n"
		"    hoping the application does not care...\n", s,
		ntohs(old_sa.sin_port));
	return bind_to(s, addr, 0);
    }
    return err;
}

static __inline
int rebind_if_needed(int s, unsigned long dest)
{
    if (IS_MNET_IP(dest)) {
	if (SOCK_UNBOUND(s)) {
	    return bind_to(s, mn_localip, 0);
	} else if (SOCK_SYS(s)) {
	    return rebind_to(s, mn_localip);
	} else {
	    return 0; /* not needed */
	}
    } else {
	if (SOCK_UNBOUND(s)) {
	    return bind_to(s, INADDR_ANY, 0);
	} else if (SOCK_MNET(s)) {
	    return rebind_to(s, INADDR_ANY);
	} else {
	    return 0; /* not needed */
	}
    }
}

int socket(int domain, int type, int protocol)
{
    int s;
    s = f_socket(domain, type, protocol);
    /* very strange: we do not seem to intercept some socket() calls. ugh. */
    DEB( fprintf(stderr, "mn: socket(%s, %s, %s) = %d\n",
		    (domain == PF_INET)?"PF_INET":"PF_??",
		    (type == SOCK_STREAM)?"SOCK_STREAM":"SOCK_DGRAM",
		    (protocol == IPPROTO_TCP)?"IPPROTO_TCP":
		    (protocol == IPPROTO_UDP)?"IPPROTO_UDP":"IPPROTO_ANY", s);)
    if (s >= 0 && domain == PF_INET) {
	assert(s < FD_SETSIZE); /* limitation of library state */
	FD_SET(s, &mn_unbound_sockets);
	FD_CLR(s, &mn_modelnet_sockets);
	FD_CLR(s, &mn_hasport_sockets);
    }
    return s;
}

/* only used for AF_UNIX sockets */
/* int socketpair(int d, int type, int protocol, int sv[2]) */

int bind(int s, const struct sockaddr *my_addr, socklen_t addrlen)
{
    struct sockaddr_in *sa;

    if (IS_INET_ADDR(my_addr, addrlen)) {
	sa = (struct sockaddr_in *)my_addr;
	DDB ( fprintf(stderr, "mn: bind(%d, %lu.%lu.%lu.%lu, %d)\n", s,
		S_ADDR_QUAD_PORT(sa)); )
	if (mn_localip) {
	    return bind_to(s, mn_localip, sa->sin_port);
	}
    }
    return f_bind(s, my_addr, addrlen);
}

int connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen)
{
    struct sockaddr_in *sa;
    struct sockaddr_in sa_magic;

    if (IS_INET_ADDR(serv_addr, addrlen)) {
	sa = (struct sockaddr_in *)serv_addr;
	DDB ( fprintf(stderr, "mn: connect(%d, %lu.%lu.%lu.%lu:%d)\n", s,
	        S_ADDR_QUAD_PORT(sa)); )

	rebind_if_needed(s, sa->sin_addr.s_addr);

        if (!mn_keep_dstip && IS_MNET_ADDR(sa) && mn_localip) {
            if (sa->sin_addr.s_addr != mn_localip) {
                /* we should not modify the application memory space */
                sa_magic = *sa;
                sa_magic.sin_addr.s_addr |= MNET_MAGIC_BIT;
                DDB ( fprintf(stderr, "mn: changed dst to %lu.%lu.%lu.%lu:%d\n",
                    S_ADDR_QUAD_PORT(&sa_magic)); )
                serv_addr = (struct sockaddr *)&sa_magic;
                addrlen = sizeof(sa_magic);
            }
        }
    }
    return f_connect(s, serv_addr, addrlen);
}

int getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    int ret;

    DEB( fprintf(stderr, "mn: getsockname(%d, ...)\n", s); )

    ret = f_getsockname(s, name, namelen);
#if 0 /* not needed: sock name will always be 10.1.x.x */
    if (IS_INET_ADDR(name, *namelen)) {
    struct sockaddr_in *sa;
	sa = (struct sockaddr_in *)name;
	DEB( fprintf(stderr, "mn: getsockname(%d, %lu.%lu.%lu.%lu:%d)\n", s,
		S_ADDR_QUAD_PORT(sa)); )
	if (!mn_keep_dstip && IS_MNET_ADDR(sa)) {
	    DEB( fprintf(stderr, "mn: getsockname(%d) unset magic bit\n", s); )
	    sa->sin_addr.s_addr &= ~MNET_MAGIC_BIT;
	}
    }
#endif
    return ret;
}

int getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
    struct sockaddr_in *sa;
    int ret;

    ret = f_getpeername(s, name, namelen);
    if (IS_INET_ADDR(name, *namelen)) {
	sa = (struct sockaddr_in *)name;
	DEB( fprintf(stderr, "mn: getpeername(%d, %lu.%lu.%lu.%lu:%d)\n", s,
		S_ADDR_QUAD_PORT(sa)); )
	if (!mn_keep_dstip && IS_MNET_ADDR(sa)) {
	    DEB( fprintf(stderr, "mn: getpeername(%d) unset magic bit\n", s); )
	    sa->sin_addr.s_addr &= ~MNET_MAGIC_BIT;
	}
    }
    return ret;
}

/* int send(int s, const void *msg, size_t len, int flags); */
/* int recv(int s, void *buf, size_t len, int flags); */

/* sendto() -- this COULD actually be TCP, which is supposed to
 * disconnect, connect, send, ack, disconnect, and reconnect. That
 * is pure crazyness. We just assume that this is UDP, in which case
 * we need to bind if it is unbound. */
RET_TYPE sendto(int s, const void *msg, size_t len, int flags,
		const struct sockaddr *to, socklen_t tolen)
{
    struct sockaddr_in *sa;
    struct sockaddr_in sa_magic;

    if (IS_INET_ADDR(to, tolen)) {
	sa = (struct sockaddr_in *)to;
	DEB ( fprintf(stderr, "mn: sendto(%d, %lu.%lu.%lu.%lu:%d, ...)\n", s,
		S_ADDR_QUAD_PORT(sa)); )

	rebind_if_needed(s, sa->sin_addr.s_addr);

	if (!mn_keep_dstip && IS_MNET_ADDR(sa) && mn_localip) {
	    if (sa->sin_addr.s_addr != mn_localip) {
		/* we should not modify the application memory space */
		sa_magic = *sa;
		sa_magic.sin_addr.s_addr |= MNET_MAGIC_BIT;
		DDB ( fprintf(stderr, "mn: changed dst to %lu.%lu.%lu.%lu:%d\n",
		    S_ADDR_QUAD_PORT(&sa_magic)); )
		to = (struct sockaddr *)&sa_magic;
		tolen = sizeof(sa_magic);
	    }
	}
    }
    return f_sendto(s, msg, len, flags, to, tolen);
}

RET_TYPE recvfrom(int s, void *buf, size_t len, int flags,
		struct sockaddr *from, socklen_t *fromlen)
{
    struct sockaddr_in *sa;
    int ret;

    ret = f_recvfrom(s, buf, len, flags, from, fromlen);
    if (IS_INET_ADDR(from, *fromlen)) {
        sa = (struct sockaddr_in *)from;
        DEB( fprintf(stderr, "mn: recvfrom(%d, ..., %lu.%lu.%lu.%lu:%d)\n", s,
                S_ADDR_QUAD_PORT(sa)); )
        if (!mn_keep_dstip && IS_MNET_ADDR(sa)) {
            DEB( fprintf(stderr, "mn: recvfrom(%d) unset magic bit\n", s); )
            sa->sin_addr.s_addr &= ~MNET_MAGIC_BIT;
        }
    }
    return ret;
}

/* sendmsg() -- same deal as sendto() */
RET_TYPE sendmsg(int s, const struct msghdr *msg, int flags)
{
    struct sockaddr_in *sa;
    struct msghdr msg_magic;
    struct sockaddr_in sa_magic;

    if (IS_INET_ADDR((struct sockaddr *)msg->msg_name, msg->msg_namelen)) {
        sa = (struct sockaddr_in *)msg->msg_name;
        DEB ( fprintf(stderr, "mn: sendmsg(%d, %lu.%lu.%lu.%lu:%d, ...)\n", s,
                S_ADDR_QUAD_PORT(sa)); )

        rebind_if_needed(s, sa->sin_addr.s_addr);

        if (!mn_keep_dstip && IS_MNET_ADDR(sa) && mn_localip) {
            if (sa->sin_addr.s_addr != mn_localip) {
                /* we should not modify the application memory space */
		msg_magic = *msg;
                sa_magic = *sa;
                sa_magic.sin_addr.s_addr |= MNET_MAGIC_BIT;
                DDB ( fprintf(stderr, "mn: changed dst to %lu.%lu.%lu.%lu:%d\n",
                    S_ADDR_QUAD_PORT(&sa_magic)); )
                msg_magic.msg_name = (struct sockaddr *)&sa_magic;
                msg_magic.msg_namelen = sizeof(sa_magic);
		msg = &msg_magic;
            }
        }
    }
    return f_sendmsg(s, msg, flags);
}

RET_TYPE recvmsg(int s, struct msghdr *msg, int flags)
{
    struct sockaddr_in *sa;
    int ret;

    ret = f_recvmsg(s, msg, flags);
    if (IS_INET_ADDR((struct sockaddr *)msg->msg_name, msg->msg_namelen)) {
        sa = (struct sockaddr_in *)msg->msg_name;
        DEB( fprintf(stderr, "mn: recvmsg(%d, ..., %lu.%lu.%lu.%lu:%d)\n", s,
                S_ADDR_QUAD_PORT(sa)); )
        if (!mn_keep_dstip && IS_MNET_ADDR(sa)) {
            DEB( fprintf(stderr, "mn: recvmsg(%d) unset magic bit\n", s); )
            sa->sin_addr.s_addr &= ~MNET_MAGIC_BIT;
        }
    }
    return ret;
}

/* so far, we don't need to deal with these at all */
/* int getsockopt(int s, int level,
		int optname, void *optval, socklen_t *optlen) */
/* int setsockopt(int s, int level,
		int optname, const void *optval, socklen_t optlen) */

/* int listen(int s, int backlog) */
/* int accept(int s, struct sockaddr *addr, socklen_t *addrlen) */
/* int shutdown(int s, int how) */



/*
 * Host vname/ip/mname cache.
 * We can keep the all virtual client names indefinitely in a cache.  Or not.
 */
struct name_ip {
    char *vname;
    char *mname; /* passed back in h_alias */
    unsigned long ip;
    struct name_ip *next;
};

#define NAME_HASH_SLOTS 139
struct name_ip *name_hash[NAME_HASH_SLOTS];

static int use_mdb_hosts;
static int use_cache;
static int cache_preload;

static void
init_hostname_cache(void)
{
    char *cache_policy;

    if (!hostsfile) return; /* no mdb, no mdb_hosts */
    use_mdb_hosts = 1;

    cache_policy = getenv("MN_HOSTNAME_CACHE");
    if (!cache_policy) cache_policy="default";

    DEB(
	fprintf(stderr, "mn: using hostname cache policy: %s\n",
		cache_policy);
    )

    if (!strcmp(cache_policy, "nopreload")) {
	use_cache = 1;
	cache_preload = 0;
    } else if (!strcmp(cache_policy, "preload")) {
	use_cache = 1;
	cache_preload = 1;
    } else if (!strcmp(cache_policy, "disable")) {
	cache_preload = 0;
	use_cache = 0;
    } else if (!strcmp(cache_policy, "default")) {
	use_cache = 1;
	cache_preload = 1;
    } else {
	DDB( fprintf(stderr,
		"mn: unknown name cache policy ``%s'': using default\n",
		cache_policy); )
	use_cache = 1;
	cache_preload = 1;
    }
}

static unsigned int
hash_string(const char *str)
{
    unsigned int h = 0;
    while (*str)
        h = (h<<5)-h + *str++;
    return h;
}

static int
get_name_hash(const char *vname, unsigned long *ip, char *mname)
{
    int slot;
    struct name_ip *a, *b;

    slot = hash_string(vname) % NAME_HASH_SLOTS;
    for (a = NULL, b = name_hash[slot]; b; a = b, b = b->next) {
        if (!strcmp(b->vname, vname)) {
            if (ip) *ip = b->ip;
            if (mname) strcpy(mname, b->mname);
            /* move to front */
            if (b != name_hash[slot]) {
                a->next = b->next;
                b->next = name_hash[slot];
                name_hash[slot] = b;
            }
            return 0;
        }
    }
    return 1;
}

static int get_ip_hash(unsigned long ip, char *vname, char *mname)
{
    int slot;
    struct name_ip *a, *b;

    for (slot = 0; slot < NAME_HASH_SLOTS; slot++) {
	for (a = NULL, b = name_hash[slot]; b; a = b, b = b->next) {
	    if (b->ip == ip) {
		/* match */
		strcpy(vname, b->vname);
		strcpy(mname, b->mname);
		/* move to front */
		if (b != name_hash[slot]) {
		    a->next = b->next;
		    b->next = name_hash[slot];
		    name_hash[slot] = b;
		}
		return 0;
	    }
	}
    }
    return 1;
}

static int put_name_hash(const char *vname, unsigned long ip, char *mname)
{
    int slot;
    struct name_ip *a;

    a = malloc(sizeof(struct name_ip));
    if (!a) return 1;

    slot = hash_string(vname) % NAME_HASH_SLOTS;
    a->vname = strdup(vname);
    a->mname = strdup(mname);
    a->ip = ip;
    a->next = name_hash[slot];
    name_hash[slot] = a;
    return 0;
}

static int find_name(const char *vname, unsigned long *ip, char *mname)
{
    FILE *hosts;
    char *filename, line[1025], l_mname[256], l_vname[256];
    unsigned int a,b,c,d,found;

    if (!use_mdb_hosts) return 1;

    /* try cache first */
    if (use_cache && !get_name_hash(vname, ip, mname)) return 0;

    if (!hostsfile) return 1;
    hosts = fopen(hostsfile, "r");
    if (!hosts) {
        fprintf(stderr, "mn: cannot read hosts file: %s\n", filename);
        free(filename);
        use_mdb_hosts = 0;
        return 1;
    }

    /* line by line */
    found = 0;
    while (fgets(line, sizeof(line), hosts)) {
        /* ignore lines starting with '#' */
        /* DEB( fprintf(stderr, "line is: '%s'\n", line); ) */
        if (sscanf(line, "%c", l_vname) == 1
            && (l_vname[0] == '#' || isspace((int)l_vname[0]))) continue;
        if (sscanf(line, "%s %u.%u.%u.%u # on %s",
                l_vname,&a,&b,&c,&d,l_mname) != 6) {
            fprintf(stderr, "mn: bad host file format: %s\n", filename);
            free(filename);
            use_mdb_hosts = 0;
            return 1;
        }
        if (cache_preload)
            put_name_hash(l_vname, htonl(MAKE_IP(a,b,c,d)), l_mname);

        if (!strcmp(vname, l_vname)) {
            /* match */
            strcpy(mname, l_mname);
            *ip = htonl(MAKE_IP(a,b,c,d));
            found = 1;
            if (!cache_preload) {
                if (use_cache)
                    put_name_hash(vname, *ip, mname);
                return 0;
            }
        }
    }
    return !found;
}

static int find_ip(unsigned long ip, char *vname, char *mname)
{
    FILE *hosts; 
    char *filename, line[1025], l_mname[256], l_vname[256];
    unsigned int a,b,c,d,found;
    
    if (!use_mdb_hosts) return 1;
    
    /* try cache first */
    if (use_cache && !get_ip_hash(ip, vname, mname)) return 0;

    /* look at mdb/hosts */
    if (!hostsfile) return 1;
    hosts = fopen(hostsfile, "r");
    if (!hosts) {
        fprintf(stderr, "mn: cannot read hosts file: %s\n", filename);
        free(filename);
        use_mdb_hosts = 0;
        return 1;
    }
    
    /* line by line */ 
    found = 0;
    while (fgets(line, sizeof(line), hosts)) {
        /* ignore lines starting with '#' */
        if (sscanf(line, "%c", l_vname) == 1
            && (l_vname[0] == '#' || isspace((int)l_vname[0]))) continue;
        if (sscanf(line, "%s %u.%u.%u.%u # on %s",
                l_vname,&a,&b,&c,&d,l_mname) != 6) {
            fprintf(stderr, "mn: bad host file format: %s\n", filename);
            free(filename);
            use_mdb_hosts = 0;
            return 1;
        }
        if (cache_preload)
            put_name_hash(l_vname, htonl(MAKE_IP(a,b,c,d)), l_mname);
        if (htonl(MAKE_IP(a,b,c,d)) == ip) {
            /* match */
            strcpy(vname, l_vname);
            strcpy(mname, l_mname);
            found = 1;
            if (!cache_preload) {
                if (use_cache)
                    put_name_hash(vname, ip, mname);
                return 0;
            }
        }
    }
    return !found;
}


struct hostent *gethostbyname(const char *name)
{
    static struct in_addr host_addr;
    static char *host_addr_list[2] = { (char *)&host_addr, NULL };
    static char host_alias[256]; /* mname */
    static char *host_aliases[] = { host_alias, NULL };
    static char host_name[256];
    static struct hostent host = {
            host_name,
            host_aliases,
            AF_INET,
            sizeof(host_addr),
            host_addr_list };

    unsigned long ip;
    char mname[256];
    const char *vname;

    if (!strcmp(name, "localhost") && mn_localhost)
	vname = mn_localhost; /* "localhost" --> "clientXX" */
    else
	vname = name;

    if (!find_name(vname, &ip, mname)) {
        /* copy data */
        strncpy(host_name, vname, sizeof(host_name));
        strncpy(host_alias, mname, sizeof(host_alias));
        host_addr.s_addr = ip;

        /* reset the pointers, just to be sure */
        host.h_name = host_name;
        host.h_aliases = host_aliases;
        host.h_aliases[0] = host_alias;
        host.h_aliases[1] = NULL;
        host.h_addrtype = AF_INET;
        host.h_length = sizeof(host_addr);
        host.h_addr_list = host_addr_list;
        host.h_addr_list[0] = (char *)&host_addr;
        host.h_addr_list[1] = NULL;

        DEB( printf("resolved %s to %lu.%lu.%lu.%lu (alias %s)\n",
            name, IP_ADDR_QUAD_N(ip), mname); )

        return &host;
    }
    return f_gethostbyname(name);
}

struct hostent *gethostbyname2(const char *name, int af)
{
    if (af == AF_INET) return gethostbyname(name);
    else return f_gethostbyname2(name, af);
}

#ifdef __GLIBC__
struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type)
#else
#ifdef __FreeBSD__
struct hostent  *gethostbyaddr(const char *addr, int len, int type)
#endif /*  __FreeBSD__ */
#endif /*  __GLIBC__ */
{
    static struct in_addr host_addr;
    static char *host_addr_list[2] = { (char *)&host_addr, NULL };
    static char host_alias[256]; /* mname */
    static char *host_aliases[] = { host_alias, NULL };
    static char host_name[256];
    static struct hostent host = {
            host_name,
            host_aliases,
            AF_INET,
            sizeof(host_addr),
            host_addr_list };

    unsigned long ip;
    char vname[256];
    char mname[256];

    if (type == AF_INET && len >= sizeof(struct in_addr)) {
	ip = ((struct in_addr *)addr)->s_addr;
	if (!find_ip(ip, vname, mname)) {
	    /* copy data */
	    strncpy(host_name, vname, sizeof(host_name));
	    strncpy(host_alias, mname, sizeof(host_alias));
	    host_addr.s_addr = ip;

	    /* reset the pointers, just to be sure */
	    host.h_name = host_name;
	    host.h_aliases = host_aliases;
	    host.h_aliases[0] = host_alias;
	    host.h_aliases[1] = NULL;
	    host.h_addrtype = AF_INET;
	    host.h_length = sizeof(host_addr);
	    host.h_addr_list = host_addr_list;
	    host.h_addr_list[0] = (char *)&host_addr;
	    host.h_addr_list[1] = NULL;

	    DEB( printf("resolved %lu.%lu.%lu.%lu to %s (alias %s)\n",
		IP_ADDR_QUAD_N(ip), vname, mname); )

	    return &host;
	}
    }
    return f_gethostbyaddr(addr, len, type);
}

#ifdef __GLIBC__
int gethostname(char *name, size_t namelen)
#else
#ifdef __FreeBSD__
int gethostname(char *name, int namelen)
#endif /*  __FreeBSD__ */
#endif /*  __GLIBC__ */
{
    if (mn_localhost) {
	strncpy(name, mn_localhost, namelen);
	return 0;
    }
    return f_gethostname(name, namelen);
}

/* these are not really ready yet, and are not much used anyway */
/* struct hostent *getipnodebyname(const char *name, int af,
		int flags, int *err); */
/* struct hostent *getipnodebyaddr(const void *addr, size_t len,
		int af, int *err); */
/* int getaddrinfo(const char *node, const char *service,
		const struct addrinfo *hints, struct addrinfo **res); */

