/************
 traffic generates multiple tcp flows at specified times for specified durations to specified targets.  The flows can be throttled or unlimited.  Used with "server".
************/
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <set>
#include <queue>
#include <vector>
#include <string>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

double getTime()
{
    struct timeval tv;
    gettimeofday(&tv,0);
    return tv.tv_sec+tv.tv_usec/1000000.0;
}

int setNonBlocking(int s)
{
    int flags = fcntl(s, F_GETFL);
    if (flags == -1)
	return false;
    return (fcntl(s, F_SETFL, flags|O_NONBLOCK) == 0);
}

class Flow;
struct QueueEntry
{
    QueueEntry(double T, Flow* f) {t=T; flow=f;}
    double t;
    Flow* flow;
};

struct QueueComparator
{
    bool operator()(const QueueEntry& a, const QueueEntry& b) const {
	return a.t > b.t;
    }
};

typedef priority_queue<QueueEntry,vector<QueueEntry>,QueueComparator> DelayQueue;


#define SEND_BUF_SIZE  1400
int sendBuffer[SEND_BUF_SIZE/sizeof(int)];

class Flow
{
public:
    Flow()
    {
	_id = 0; 
	_s=-1; 
	_duration=-1; 
	_rate=-1; 
	_transferCap=-1; 
	_sent = 0; 
	_dst = 0;
	_port = 0;
    }
    void setID(int id) {_id=id;}
    void setDestination(unsigned long dst){_dst = dst;}
    void setPort(unsigned short port) {_port = port;}
    void setStartTime(double start) {_start = start;}
    void setDuration(double duration) {_duration = duration;}
    void setRate(double rate) {_rate = rate;}
    void setTransferCap(int bytes) {_transferCap = bytes;}
    int getSocket() const {return _s;}
    int getID() const {return _id;}
    unsigned long getDestination() const {return _dst;}
    unsigned short getPort() const {return _port;}
    double getStartTime() const {return _start;}
    double getDuration() const {return _duration;}
    double getRate() const {return _rate;}
    int getTransferCap() const {return _transferCap;}

    bool isConnected() const {return _s != -1;}
    int connect()
    {
	if (_dst == 0 || _port == 0 || _s != -1)
	    return false;
	_s = socket(AF_INET, SOCK_STREAM, 0);
	if (!setNonBlocking(_s))
	    return false;
	sockaddr_in saddr;
	memset(&saddr,0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(_port);
	saddr.sin_addr.s_addr = _dst;
	if (::connect(_s, (sockaddr*)&saddr, sizeof(saddr)) == -1)
	{
	    return (errno == EINPROGRESS);
	}
	return true;
    }

    int service(DelayQueue& q, double now)
    {
	if (_s == -1)
	    return false;
	sendBuffer[0] = _id;
	sendBuffer[1] = sizeof(sendBuffer)-2*sizeof(int);
	int sent = send(_s, sendBuffer, SEND_BUF_SIZE, 0);
	if (sent == -1)
	{
	    if (errno == EAGAIN || errno == EWOULDBLOCK)
		q.push(QueueEntry(now+0.000001, this));
	    else
		cerr << "error: " << errno << endl;
	    return true;
	}
	//if (sent != SEND_BUF_SIZE)
	//   cout << "sent " << sent << endl;
	_sent += sent;
	_lastSendTime = now;
	_lastSendSize = sent;
	double delay = _rate > 0 ? sent/_rate : 0.0000001;
	q.push(QueueEntry(now+delay,this));
	return true;
    }

    bool isExpired(double now) const
    {
	if (_transferCap >= 0 && _sent >= _transferCap)
	    return true;
	if (_duration >= 0 && (now-_start) >= _duration)
	    return true;
	return false;
    }

    int destroy(double now)
    {
	_duration = now -_start;
	if (_s == -1)
	    return false;
	shutdown(_s, 1);
	close(_s);
	_s = -1;
	return true;
    }

    int getTransferred() const {return _sent;}
    double getObservedRate(double now) const 
    {
	if (_duration > 0 && _start+_duration < now)
	    return (_sent/_duration);
	else
	    return _sent/(now-_start);
    }

private:
    int _s;
    int _id;
    unsigned long _dst;
    unsigned short _port;
    double _start;
    double _duration;
    double _rate;
    int _transferCap;
    int _sent;
    double _lastSendTime;
    int _lastSendSize;
};


void checkPending(set<Flow*>& pending, vector<Flow*>& connected)
{
    fd_set writeSet;
    FD_ZERO(&writeSet);
    set<Flow*>::iterator it;
    int maxfd=0;
    for (it = pending.begin(); it != pending.end(); it++)
    {
	FD_SET((*it)->getSocket(), &writeSet);
	if ((*it)->getSocket() > maxfd)
	    maxfd = (*it)->getSocket();
    }
    timeval timeout={0,0};
    if (select(maxfd+1, 0, &writeSet, 0, &timeout) == 0)
	return;
    for (it = pending.begin(); it != pending.end(); it++)
    {
	if (FD_ISSET((*it)->getSocket(), &writeSet))
	    connected.push_back(*it);
    }
}

void usage(const char* prog)
{
    cout << "Usage: " << prog << " <flow decl file> " << endl;
    cout << "Flow file (one flow per line): " << endl;
    cout << "\t" << " <id> <dst_ip> <dst_port> <startTime> <dur> <rate in kbit/s> <transferCap>" << endl;
    cout << " with negative values for dur, rate, and transferCap disabling them." << endl;
    exit(1);
}

bool verbose = false;
int main(int argc, char* argv[])
{
    DelayQueue q;
    set<Flow*> pending, connected, finished;
    set<Flow*>::iterator it;

    if (argc != 2)
	usage(argv[0]);
    ifstream in(argv[1]);
    if (in.fail())
    {
	cout << "Error opening file: " << argv[1] << endl;
	usage(argv[0]);
    }

    int id, port, transferCap;
    double startTime, duration, rate;
    struct hostent* host;
    double t0 = getTime();
    while (in >> id)
    {
	string dst;
	if (!(in >> dst) || (host=gethostbyname(dst.c_str())) == 0)
	{
	    cerr << "Invalid destination: " << dst << endl;
	    usage(argv[0]);
	}
	if (!(in >> port >> startTime >> duration >> rate >> transferCap))
	{
	    cerr << "Invalid file format." << endl;
	    usage(argv[0]);
	}
	Flow* f = new Flow;
	f->setID(id);
	f->setDestination(*(unsigned long*)host->h_addr_list[0]);
	f->setPort(port);
	f->setStartTime(startTime+t0);
	f->setDuration(duration);
	f->setRate(rate*128);
	f->setTransferCap(transferCap);
	q.push(QueueEntry(f->getStartTime(),f));
    }
    in.close();

    double now, dt;
    while (pending.size() || q.size())
    {
	now = getTime();
	int cycles = 0;
	if (pending.size())
	{
	    vector<Flow*> newConnections;
	    checkPending(pending, newConnections);
	    for (size_t i=0; i<newConnections.size(); i++)
	    {
		if (verbose) cout << "connected " << newConnections[i]->getID() << endl;
		pending.erase(newConnections[i]);
		connected.insert(newConnections[i]);
		newConnections[i]->setStartTime(now);
		newConnections[i]->service(q,now);
	    }
	}
	now=getTime();
	if (q.size() && (dt=(q.top().t-now)) > 0.011)
	{
	    if (dt > 1) dt = 1;
	    struct timespec ts;
	    ts.tv_sec = 0;
	    ts.tv_nsec = (long)(dt*1000000000);
	    nanosleep(&ts,0);
	}
	else while (q.size() && q.top().t <= now && cycles++ < 50)
	{
	    Flow* f = q.top().flow; 
	    q.pop();
	    if (!f->isConnected())
	    {
		if (verbose) cout << "connect "<< f->getID() << endl;
		f->connect();
		pending.insert(f);
	    }
	    else if (f->isExpired(now))
	    {
		if (verbose) cout << "expire " << f->getID() << endl;
		f->destroy(now);
		connected.erase(f);
		finished.insert(f);
	    }
	    else
		f->service(q, now);
	    now = getTime();
	}
    }
    now = getTime();
    //for (it = finished.begin(); it != finished.end(); it++)
    //{
    //cout << (*it)->getID() << ": " << (*it)->getObservedRate(now)*8/1024.0 << " ";
    //delete *it;
    //}
    //cout << endl;
    cout << "Finished "<< finished.size() << " flows." << endl;
    return 0;
}
