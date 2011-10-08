/************
 blaster makes multiple unthrottled tcp flows to the specified endpoints, for a given time period.
************/

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <set>
#include <vector>
#include <string>
#include <errno.h>
#include <time.h>

const int BUFSIZE = 2048;
char buffer[BUFSIZE];
set<int> sockets;
bool locked = false;
bool stop = false;
struct timespec nanotime = {0,10000000};

void error(string msg)
{
    cerr << "Error: " << msg << endl;
    exit(1);
}

void usage()
{
    cerr << "usage: blaster [-p port] [-l seconds] [-s packetsize] host1 host2 ... hostn " << endl;
    exit(1);
}

int main(int argc, char* argv[])
{
  if (argc <2)
    usage();
  int packetSize = 1460;
  unsigned short port = 22222;
  int seconds = 10;
  string hostnames = argv[1];
  vector<string> hosts;
  unsigned int i;
  for (i=1; i<(unsigned int)argc; i++) {
    if (string(argv[i]) == "-p") {
      if ((int)i == argc-1)
	usage();
      port = atoi(argv[i+1]);
      i++;
    }
    else if (string(argv[i]) == "-l") {
      if ((int)i == argc-1)
	usage();
      seconds = atoi(argv[i+1]);
      i++;
    }
    else if (string(argv[i]) == "-s") {
      if ((int)i == argc-1)
	usage();
      packetSize = atoi(argv[i+1]);
      i++;
    }
    else
      hosts.push_back(argv[i]);
  }
  vector<int> socks(hosts.size());
  for (i=0;i<socks.size();i++) {
    socks[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socks[i] == -1)
      error("cannot create socket");
    int trueVal=1;
    if (setsockopt(socks[i], SOL_SOCKET, TCP_NODELAY, &trueVal, sizeof(trueVal))==-1)
      error("cannot set socket options");
    struct hostent* host = gethostbyname(hosts[i].c_str());
    if (host == 0)
      error("host not found: "+hosts[i]);
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = *(unsigned long*)host->h_addr_list[0];
    saddr.sin_port= htons(port);
    while (connect(socks[i], (struct sockaddr*)&saddr, sizeof(saddr)) == -1)
      sleep(1);
  }
  // cout << "blasting..." << endl;
  int startTime = time(NULL);
  int total = 0;
  while (time(NULL) - startTime < seconds) {
    for (i=0;i<socks.size();i++) {
      int ret = send(socks[i], buffer, packetSize, 0);
      if (ret == -1) {
	if (errno == ENOBUFS)
	  nanosleep(&nanotime,0);
	else
	  error("send error.");
      }
      else
	total += ret;
    }
  }
  for (i=0;i<socks.size();i++) {
    shutdown(socks[i],1);
    close(socks[i]);
  }
  cout << "rate: "<< (total/seconds)/1000.0 << " KB/s" << endl;
  return 0;
}
