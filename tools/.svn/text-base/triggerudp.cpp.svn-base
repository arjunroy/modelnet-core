#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>

void usage()
{
  cerr << "triggerudp <ip> <portnum>" <<endl;
  exit(1);
}

void error(string msg)
{
  cerr << "Error: "<<msg<<endl;
  exit(1);
}

int main(int argc, char* argv[])
{
  if (argc != 3)
    usage;
  int port = atoi(argv[2]);
  if (port<0 || port> 65535)
  {
    cerr << "Bad port number." <<endl;
    usage();
  }
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == -1)
    error("Could not create socket.");
  int trueVal = 1;
  if (setsockopt(sock,SOL_SOCKET, SO_BROADCAST, &trueVal, sizeof(trueVal)) == -1)
    error("Could not broadcast.");
  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons((unsigned short)port);
  saddr.sin_addr.s_addr = inet_addr(argv[1]);
  char buf[]="trigger";
  int bytes = sendto(sock, buf, strlen(buf), 0, (struct sockaddr*)&saddr, sizeof(saddr));
  if (bytes == -1)
    error("Send failed.");
  return 0;
}
