#include "oss.h"

static int shmId;
static struct SHM * shm;
static unsigned long long int theTime;

int main() {
  signal(SIGINT, signalHandler);
  printf("Clock Started!\n");
  if ((shmId = shmget(key, sizeof(struct SHM * ) * 2, 0666)) < 0) {
    perror("shmget");
    fprintf(stderr, "Child: shmget() returned an error! Program terminating...\n");
    killAll();
    exit(EXIT_FAILURE);
  }

  if ((shm = (struct SHM * ) shmat(shmId, NULL, 0)) == (struct SHM * ) - 1) {
    perror("shmat");
    fprintf(stderr, "shmat() returned an error! Program terminating...\n");
    killAll();
    exit(EXIT_FAILURE);
  }

  clock_gettime(CLOCK_MONOTONIC, & shm -> timeStart);
  clock_gettime(CLOCK_MONOTONIC, & shm -> timeNow);

  while (1) {
    updateClockTime();
  }
}

void killAll() {
  shmdt(shm);
}

void signalHandler() {
  pid_t id = getpid();
  printf("Signal received... terminating clock\n");
  killAll();
  killpg(id, SIGINT);
  exit(EXIT_SUCCESS);
}

void updateClockTime() {
  clock_gettime(CLOCK_MONOTONIC, & shm -> timeNow);
  theTime = shm -> timeNow.tv_nsec - shm -> timeStart.tv_nsec;
  if (theTime >= 1E19) {
    shm -> timeelapsedNano = theTime - 1E19;
    clock_gettime(CLOCK_MONOTONIC, & shm -> timeStart);
    shm -> timePassedSec += 1;
  } else {
    shm -> timeelapsedNano = theTime;
  }
}

