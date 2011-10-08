/**************
 Server is a simple tcp server that accepts multiple connections and reads data off the connections.  Usually used in conjunction with "blaster" or "traffic".
**************/

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <set>
#include <vector>
#include <string>
#include <time.h>


const int BUFSIZE = 2048;
char buffer[BUFSIZE];
int server;
set<int> sockets;
bool locked = false;
bool stop = false;
struct timespec nanotime = {0,10000000};

inline void lock()
{
    while (locked)
	nanosleep(&nanotime, 0);
    locked=true;
}

inline void unlock()
{
    locked = false;
}

void setNonblocking(int s)
{
    int flags = fcntl(s, F_GETFL,0);
    fcntl(s, F_SETFL, flags|O_NONBLOCK);
}

void think()
{
    fd_set readSet;
    struct timeval tv={1,0};
    unsigned int i;
    vector<int> toRemove;
    int maxfd = server;
    FD_ZERO(&readSet);
    FD_SET(server,&readSet);
    set<int>::iterator it;
    for (it=sockets.begin();it!=sockets.end();it++)
    {
	FD_SET(maxfd = *it, &readSet);
    }
    int sel = select(maxfd+1, &readSet,0,0,&tv);
    if (sel <= 0)
    {
	struct timespec ts={0,100000000};
	nanosleep(&ts,0);
    }
    if (FD_ISSET(server,&readSet))
    {
	int s = accept(server, 0, 0);
	if (s != -1 && sockets.size() < FD_SETSIZE)
	{
	    setNonblocking(s);
	    sockets.insert(s);
	}
    }
    for (it=sockets.begin();it!=sockets.end();it++)
    {
	if (FD_ISSET(*it, &readSet))
	{
	    int ret = recv(*it, buffer, BUFSIZE, 0);
	    if (ret <= 0)
		toRemove.push_back(*it);
	}
    }
    for (i=0;i<toRemove.size(); i++)
    {
	shutdown(toRemove[i],1);
	close(toRemove[i]);
	sockets.erase(toRemove[i]);
    }
}

void error(string msg)
{
    cerr << "Error: " << msg << endl;
    exit(1);
}

void usage()
{
    cerr << "usage: server [-p port]" << endl;
    exit(1);
}


int main(int argc, char* argv[])
{
    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == -1)
	error("cannot create socket.");
    setNonblocking(server);
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = argc > 1 ? htons(atoi(argv[argc-1])) : htons(22222);
    saddr.sin_addr.s_addr = 0;
    if (bind(server, (struct sockaddr*)&saddr, sizeof(saddr)) == -1)
	error("cannot bind.");
    if (listen(server,1) == -1)
	error("cannot listen.");
    // cerr << "Starting server..." << endl;
    while (true)
    {
	think();
    }
}
