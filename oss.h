
#ifndef PROJECT4_H
#define PROJECT4_H

#include <ctype.h>

#include <errno.h>

#include <signal.h>

#include <stdint.h>

#include <stdio.h>

#include <stdlib.h>

#include <sys/ipc.h>

#include <sys/msg.h>

#include <sys/shm.h>

#include <sys/types.h>

#include <time.h>

#include <unistd.h>

#define prioritySetting 10 // raising this number increases the chance of a process being created and inserted into queue 1 regardless of quantum. Percent chance = 10% chance
#define IOSetting 5 // this number is multipled by 1000000000 to give a time in nanoseconds -- Lower number = more frequent interupts -- Higher number reduces frequency, but does not eliminate them
#define sleepTime 5 // this number is multiplied by 10,000 to give a process sleep time in microseconds. DEFAULT: 5 = .05 second sleep per cycle (spread out over cycle)
#define maxSectionlen 5 // Quantum length is randomly chosen, but this number is the maximum value allowed (in seconds)
#define maxProc 18 // changes max number of children process spawns
#define MessageBuffer 64
#define spawnRateSetting 3 // how frequent processes spawn -- calculated by rand() % spawnRateSetting and then sleeping for the result in seconds

struct msg_buf {
  long mtype;
  char mtext[MessageBuffer];
};

struct PCB {
  int alive;
  int creationSec;
  int finishSec;
  int index;
  int inQueueSec;
  int moveFlag;
  int pid;
  int ProcessNum;
  int queue;

  unsigned long long int cpuTime;
  unsigned long long int creationNansec;
  unsigned long long int finishNansec;
  unsigned long long int inQueueNansec;
  unsigned long long int Quantum;
};

struct ProcessQueue {
  int index[maxProc];
  int numProc;
  int pid[maxProc];
};

struct SHM {
  int childControl;
  int flag[maxProc];
  int Schd_Cnt;
  int schedule[maxProc];
  int timePassedSec;
  int turn;

  pid_t pid;

  struct timespec timeStart, timeNow, timePassed;

  unsigned long int timeelapsedNano;
};

enum state {
  idle,
  want_in,
  in_cs
};

int createProcess(int);
int DeQueue(int, pid_t);

key_t CSKey = 3868970;
key_t key = 5782000;
key_t PCBKey = 4031609;
key_t UserKey = 2239686;
key_t OSSKey = 2771116;

long getRightTime();

void EnQueue(int, pid_t, char * );
void advanceQueues();
void ClearPBlock(int);
void printProcessStats(int);
void killAll();
void printStats();
void scheduleProcess();
void signalHandler();
void updateClockTime();
int getIndex();

#endif

