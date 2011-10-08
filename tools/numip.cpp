#include <iostream>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

int main(int argc, char* argv[])
{
    char buf[500];
    if (argc == 1)
	gethostname(buf,499);
    else
	strcpy(buf, argv[1]);
    struct hostent* host = gethostbyname(buf);
    if (host == 0)
    {
	cerr << "Cannot find host." << endl;
	exit(1);
    }
    cout << "Host: " << host->h_name << endl;
    unsigned long localhost = inet_addr("127.0.0.1");
    unsigned long ip = localhost;
    int i=0;
    while (ip == localhost && ip!= 0)
	ip = *(unsigned long*)host->h_addr_list[i++];
    unsigned char* bytes = (unsigned char*)&ip;
    for (i=0;i<4;i++)
    {
	cout << (int) bytes[i];
	if (i<3)
	    cout << ".";
    }
    cout << "\t" << ip << " (" << int(ip) << ")" <<endl;
    return 0;
}
