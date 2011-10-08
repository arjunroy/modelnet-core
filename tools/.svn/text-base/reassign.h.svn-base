#ifndef __REASSIGN_H_
#define __REASSIGN_H_

#pragma pack(4)
struct ReassignHeader
{
    int stages;
};

struct ReassignStage
{
    long count;
};

struct ReassignEntry
{
    long pipe_nr;
    unsigned long owner;
};

enum ReassignCmdType {FLUSH_STATS, START_STAGE, FINISHED, EXIT, OK};
struct ReassignCmd
{
    long cmd;
    long arg;
};

#pragma pack()
#endif
