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
  cerr << "waitudp <portnum>" <<endl;
  exit(1);
}

void error(string msg)
{
  cerr << "Error: "<<msg<<endl;
  exit(1);
}

int main(int argc, char* argv[])
{
  if (argc != 2)
    usage;
  int port = atoi(argv[1]);
  if (port<0 || port> 65535)
  {
    cerr << "Bad port number." <<endl;
    usage();
  }
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == -1)
    error("Could not create socket.");
  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons((unsigned short)port);
  saddr.sin_addr.s_addr = 0;
  if (bind(sock, (struct sockaddr*)&saddr, sizeof(saddr)) == -1)
    error ("Could not bind.");
  int bytes;
  unsigned int namelen = sizeof(saddr);
  char buffer[100];
  bytes = recvfrom(sock, buffer, 100, 0, (struct sockaddr*)&saddr, &namelen);
  if (bytes == -1)
    error("recv error.");
  return 0;
}
