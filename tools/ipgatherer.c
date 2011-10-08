/*
 * adapted from: netdump_server.c
 * user level server that accepts netdump network dumps.
 *
 * Copyright (c) 2000 Darrell Anderson <anderson@cs.duke.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/errno.h>
#define __USE_XOPEN
#include <sys/poll.h>
#include <time.h>
struct net_log_gatherer_msg {
    int node;
    unsigned int lsn;
};

#include <net_log_format.h>

struct netlog_msg {
        struct net_log_msg 	msg;
        unsigned char 		data[8192];   /* real message may contain less */
};



/*
 * [ip2name]
 *
 * translate a host ip address to a hostname.  exit verbosely on error.
 *
 * Parameters:
 *      ip      ip address (network byte order)
 *
 * Returns:
 *      char *  hostname (null-terminated ascii string)
 */
static char *
ip2name(struct in_addr ip)
{
	static char name[128];
	struct hostent *he;
	char *dot;
	long int a = ntohl(ip.s_addr);

	sprintf(name, "%d.%d.%d.%d", 
		(int)(a>>24)&0xff, (int)(a>>16)&0xff,
		(int)(a>>8)&0xff, (int)(a)&0xff);
	return name;

	if ((he = gethostbyaddr((caddr_t)&ip, sizeof(ip), AF_INET)) == NULL) {
		fprintf(stderr, "can't resolve %d.%d.%d.%d\n",
			(int)(a>>24)&0xff, (int)(a>>16)&0xff,
			(int)(a>>8)&0xff, (int)(a)&0xff);
		exit(1);
	}
	strncpy(name, he->h_name, sizeof(name));
	if ((dot = strchr(name, '.')) != NULL) {
		*dot = '\0';
	}
	return name;
}

/*
 * [name2fd]
 *
 * given a hostname, generate a file descriptor for its vmcore file.
 *
 * Parameters:
 *      name    hostname (null-terminated ascii string)
 *
 * Returns:
 *      int     file descriptor for vmcore file
 */
#ifdef NOT_USED_ANYWHERE
static char *prefix;
static char dumpname[128];

static int
name2fd(char *name)
{
	struct stat sb;
	int fd, i;

	for (i=0 ; i<256 ; i++) {
		snprintf(dumpname, sizeof(dumpname), "%s/vmcore.%s.%d", 
			 prefix, name, i);
		if (stat(dumpname, &sb) == 0) {
			continue; /* file existed, proceed. */
		}
		if (errno != ENOENT) {
			perror("stat"); /* unexpected error, die. */
			exit(1);
		}
		if ((fd = open(dumpname, O_WRONLY|O_CREAT, DEFFILEMODE)) == -1) {
			fprintf(stderr, "open failed for \"%s\"\n", dumpname);
			perror("open");
			exit(1);
		}
		return fd;
	}
	printf("too many %s/%s.vmcore.X files!\n", prefix, name);
	exit(1);
	return -1; /* unreachable -- stop compiler warnings. */
}
#endif

/*
 * [sockaddr_init]
 *
 * fill in a sockaddr given its component fields.
 *
 * Parameters:
 *      saddr   sockaddr
 *      ipaddr  ip address (network byte order)
 *      port    port (network byte order)
 *
 * Returns:
 *      void
 */
static void
sockaddr_init(struct sockaddr_in *saddr, u_int32_t ipaddr, short port)
{
	bzero(saddr, sizeof(struct sockaddr_in));
	saddr->sin_family = AF_INET;
	/*saddr->sin_len = sizeof(struct sockaddr_in);*/
	saddr->sin_port = port;
	saddr->sin_addr.s_addr = ipaddr;
}

char log_path[128] = "./";
unsigned int write_binary = 1;
unsigned int verbose = 1;

static void 
check_for_close(int * filedes, short * goodtimeleft)
{
	int i;
	for (i=0; i<256; i++)
		if (filedes[i] > -1) {
			goodtimeleft[i]--;
			if ( goodtimeleft[i] <= 0 ) {
				if (verbose) {
					printf("Closing log file for node %d. [inactive]\n", i);
					fflush(stdout);
				}
				close (filedes[i]);
				filedes[i] = -1;
			}
		}
	
	return;
}


/*
 * [serverloop]
 *
 * main program loop.  open a connectionless socket listening on the
 * netlog port and loop receiving packets from anyone.  process each
 * packet independently.
 *
 * Parameters:
 *      void
 *
 * Returns:
 *      void
 */

#define FILEOPENTIME  15  /*approx seconds before an inactive file is closed */

static void
serverloop(void)
{
	int sock, fd = 0, cnt, written, rcvflag = 0/*MSG_WAITALL*/;
	int i, pollret, filedes[256]; 
	short goodtimeleft[256];
	struct sockaddr_in local, frm;
	struct in_addr cached_ip;
	u_int32_t frmlen;
	struct netlog_msg *nd_msg;
	struct net_log_msg *msg;
	char *name = NULL;
	char filename[128];
	time_t oldtime = time(NULL);
	struct pollfd pfd;
	
	for ( i = 0; i < 256; i++ ) {
		filedes[i] = -1;  /* initialize fd's to invalid values */
	}

	nd_msg = (struct netlog_msg *)malloc(sizeof(struct netlog_msg));

	if (nd_msg == NULL) {
		perror("malloc");
		exit(1);
	}
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		perror("socket");
		exit(1);
	}
	sockaddr_init(&local, INADDR_ANY, htons(NET_LOG_PORT));
	if (bind(sock, (struct sockaddr *)&local, sizeof(local)) == -1) {
		perror("bind");
		exit(1);
	}
	printf("netlog_server listening on port %d, writing logs to %s\n",
	       NET_LOG_PORT, log_path);
	fflush(stdout);

	while (1) {

		/*
		 * get a packet
		 */
		sockaddr_init(&frm, INADDR_ANY, 0);
		frmlen = sizeof(struct sockaddr_in);

		pfd.fd = sock;
		pfd.events =  POLLRDNORM;
		pfd.revents = POLLRDNORM;

		do {
			pollret = poll(&pfd, 1, 1000);

			if (time(NULL) - oldtime >= 1 )
				check_for_close(filedes, goodtimeleft );
			oldtime = time(NULL);
		} while (pollret == 0);

		if ((cnt = recvfrom(sock, nd_msg, sizeof(*nd_msg), rcvflag, 
				    (struct sockaddr *)&frm, &frmlen)) == -1) {
			perror("recvfrom");
			exit(1);
		}
		cnt -= sizeof(struct net_log_msg);
		if ((cnt != 4096) && (cnt != 8192)) {
			printf("runt: read %d, expected 4096 or 8192\n", cnt);
			fflush(stdout);
		} 

		if (frm.sin_addr.s_addr != cached_ip.s_addr) {
			if (fd) {
				close (fd);
			}
			name = ip2name(frm.sin_addr);
			/* fd = name2fd(name);
			   cached_ip.s_addr = frm.sin_addr.s_addr;*/
		}

		msg = &nd_msg->msg;
		if ( filedes[msg->node] < 0 ) {
			sprintf( filename, "%s/log%03d", log_path, msg->node );
			if (verbose) {
			    printf( "Opening log file for node %s [%d]: %s\n",
					 name, msg->node, filename );
			    fflush(stdout);
			}
			filedes[msg->node] =
			    open( filename, O_CREAT|O_APPEND|O_WRONLY, 420 );
			if (filedes[msg->node] < 0 )
				printf("Open failed, filename: %s\n", filename);
		}	
		goodtimeleft[msg->node] = FILEOPENTIME;
		written = write( filedes[msg->node], nd_msg->data, cnt);
		if (written != cnt)
		    printf("Write failed, %s.  Tried to write %d bytes, error=%s.\n",filename, cnt, strerror(errno));
		close(filedes[msg->node]);
		filedes[msg->node]=-1;

	}
}

int
main(int argc, char *argv[])
{
	argv++; argc--;
	while (*argv != NULL) {
		if (strcmp(*argv, "--binary") == 0) {
			printf("using binary mode\n");
			fflush(stdout);
			argv++; argc--;
			write_binary = 1;
			continue;
		}
		if (strcmp(*argv, "--verbose") == 0) {
			printf("verbose mode\n");
			fflush(stdout);
			argv++; argc--;
			verbose = 1;
			continue;
		}
		if (strcmp(*argv, "--logpath") == 0) {
			argv++; argc--;
			strcpy(log_path, *argv);
			argv++; argc--;
			printf("using logpath %s\n", log_path);
			fflush(stdout);
			continue;
		}
		fprintf(stderr, "option not recognized: %s\n", *argv);
		return 1;
	}

	serverloop(); /* never returns */
	return 0;
}
