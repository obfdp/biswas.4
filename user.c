#include "oss.h"

const static unsigned long long int min = IOSetting * 100000000;

FILE * fp;

static int logfile_lines = 0;
static int send_msgid, receive_msgid, critical_msgid, shmId, pcbId;

static struct PCB( * PBlock)[maxProc];
static struct SHM * shm;

int main(int argc, char * argv[]) {

  signal(SIGINT, signalHandler);   // signal handler
  signal(SIGSEGV, signalHandler);

  char * fileName = "UserLogs.txt";
  fp = fopen(fileName, "a");
  if (fp == NULL) {
    printf("Couldn't open file");
    errno = ENOENT;
    killAll();
    exit(EXIT_FAILURE);
  }


  srand((unsigned)(getpid() ^ time(NULL) ^ ((getpid()) >> maxProc)));   // seed the random number generator


  if ((shmId = shmget(key, sizeof(struct SHM * ) * 3, 0666)) < 0) {   // get shared memory id 
    perror("shmget");
    fprintf(stderr, "Child: shmget() $ returned an error! Program terminating...\n");
    killAll();
    exit(EXIT_FAILURE);
  }


  if ((shm = (struct SHM * ) shmat(shmId, NULL, 0)) == (struct SHM * ) - 1) {   // attach the shared memory
    perror("shmat");
    fprintf(stderr, "shmat() returned an error! Program terminating...\n");
    killAll();
    exit(EXIT_FAILURE);
  }


  if ((pcbId = shmget(PCBKey, sizeof(struct PCB * ) * (maxProc * 100), 0666)) < 0) {   // get process control block id
    perror("shmget");
    fprintf(stderr, "shmget() $$ returned an error! Program terminating...\n");
    exit(EXIT_FAILURE);
  }


  if ((PBlock = (void * )(struct PCB * ) shmat(pcbId, NULL, 0)) == (void * ) - 1) {   // attach PCB
    perror("shmat");
    fprintf(stderr, "shmat() returned an error! Program terminating...\n");
    exit(EXIT_FAILURE);
  }


  if ((receive_msgid = msgget(UserKey, 0666)) < 0) {   // Attach message queues
    fprintf(stderr, "Child %i has failed attaching the sending queue\n", getpid());
    killAll();
    exit(EXIT_FAILURE);
  }

  if ((send_msgid = msgget(OSSKey, 0666)) < 0) {
    fprintf(stderr, "Child %i has failed attaching the sending queue\n", getpid());
    killAll();
    exit(EXIT_FAILURE);
  }

  if ((critical_msgid = msgget(CSKey, 0666)) < 0) {
    perror("msgget");
    killAll();
    exit(EXIT_FAILURE);
  }

  struct msg_buf recieveMsgbuff, sendMsgbuff, criticalMsgbuff;
  int i = getIndex();


  backToWait: 
    while (msgrcv(receive_msgid, & recieveMsgbuff, MessageBuffer, getpid(), 0) < 0);
  while (shm -> childControl);   // Wait to be scheduled by processor // If process does not complete in time, come back to this point
  printf("PROCESS: #%i received scheduling message @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);

  if (logfile_lines < 10000) {
    fprintf(fp, "PROCESS: #%i received scheduling message @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
    logfile_lines++;
  }

  int c, j,
  n = maxProc,
    toSleep,
    statusOne = 1,
    statusZero = 0;

 
  do {   // peterson's algorithm for more than two processes (slides)
    shm -> flag[i] = want_in;

    j = shm -> turn;     //Set local variable

    while (j != i) {
      j = (shm -> flag[j] != idle) ? shm -> turn : (j + 1) % n;
    }


    shm -> flag[i] = in_cs;     // Declare intention to enter critical section


    for (j = 0; j < n; j++) {     // Check that no one else is in critical section
      if ((j != i) && (shm -> flag[j] == in_cs)) {
        break;
      }
    }
  } while ((j < n) || ((shm -> turn != i) && (shm -> flag[shm -> turn] != idle)));


  shm -> turn = i;   // assign to self and enter critical section

  /*Critical Section*/
  if (logfile_lines < 10000) {
    fprintf(fp, "PROCESS: #%i entered critical section @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
    logfile_lines++;
  }

  sendMsgbuff.mtype = 1;
  sprintf(sendMsgbuff.mtext, "%i", getpid());

  if (msgsnd(send_msgid, & sendMsgbuff, MessageBuffer, IPC_NOWAIT) < 0) {
    perror("msgsnd");
    printf("The reply to child did not send\n");
    signalHandler();
  }


  printf("PROCESS: #%i requesting control @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);   // Request OSS to stop
  if (logfile_lines < 10000) {
    fprintf(fp, "PROCESS: #%i requesting control @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
    logfile_lines++;
  }


  while (msgrcv(critical_msgid, & criticalMsgbuff, MessageBuffer, 1, 0) < 0);   // Begin running when OSS stops
  printf("PROCESS: #%i began running @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
  if (logfile_lines < 10000) {
    fprintf(fp, "PROCESS: #%i began running @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
    logfile_lines++;
  }

  /* If Quantum > running time, this process cant finish in this iteration and must be requeued */
  unsigned long long int runningTime = rand() % (1000000000 - min) + min;
  if (PBlock[i] -> Quantum > runningTime) {
    usleep((runningTime / 1000));

    PBlock[i] -> Quantum = PBlock[i] -> Quantum - runningTime;

    sendMsgbuff.mtype = PBlock[i] -> Quantum;
    sprintf(sendMsgbuff.mtext, "%i", statusOne);

    /*Exit Critical Section*/
    printf("PROCESS: #%i was interrupted @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
    if (logfile_lines < 10000) {
      fprintf(fp, "PROCESS: #%i was interrupted @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
      logfile_lines++;
    }

    j = (shm -> turn + 1) % n;
    while (shm -> flag[j] == idle) {
      j = (j + 1) % n;
    }

    
    if (msgsnd(send_msgid, & sendMsgbuff, MessageBuffer, IPC_NOWAIT) < 0) {
      perror("msgsnd");
      printf("The reply to child did not send\n");
      signalHandler();
    }

    /* Give OSS control*/
    printf("PROCESS: #%i relinquished control @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
    if (logfile_lines < 10000) {
      fprintf(fp, "PROCESS: #%i relinquished control @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
      logfile_lines++;
    }

    /* Wait for OSS to take over*/
    while (msgrcv(critical_msgid, & criticalMsgbuff, MessageBuffer, 1, 0) < 0);

    shm -> turn = j;
    shm -> flag[i] = idle;

    /* Go back and requeue*/
    goto backToWait;

  } else {
    /*If Quantum < runningTime, the process can finish*/
    long int toSleep = PBlock[i] -> Quantum / 1000;
    usleep(toSleep);

    sendMsgbuff.mtype = 1;
    sprintf(sendMsgbuff.mtext, "%i", statusZero);

    /*Exit Critical Section*/
    printf("PROCESS: #%i completed @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
    if (logfile_lines < 10000) {
      fprintf(fp, "PROCESS: #%i completed @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
      logfile_lines++;
    }
  }

  j = (shm -> turn + 1) % n;
  while (shm -> flag[j] == idle) {
    j = (j + 1) % n;
  }

  /*EXITED*/
  if (logfile_lines < 10000) {
    fprintf(fp, "PROCESS: #%i exited critical section @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
    logfile_lines++;
  }

  /*Assign turn to next waiting process; change own flag to idle*/
  shm -> turn = j;
  shm -> flag[i] = idle;

  /*Give OSS back control*/
  printf("PROCESS: #%i quitting and relinquishing control @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
  if (logfile_lines < 10000) {
    fprintf(fp, "PROCESS: #%i quitting and relinquishing control @ %03i.%09lu\n", getpid(), shm -> timePassedSec, shm -> timeelapsedNano);
    logfile_lines++;
  }

  usleep(1000);

  if (msgsnd(send_msgid, & sendMsgbuff, MessageBuffer, IPC_NOWAIT) < 0) {
    perror("msgsnd");
    printf("The reply to child did not send\n");
    signalHandler();
  }

 
  while (msgrcv(critical_msgid, & criticalMsgbuff, MessageBuffer, 1, 0) < 0);  // Wait for OSS to take over

  PBlock[i] -> finishSec = shm -> timePassedSec;
  PBlock[i] -> alive = 0;
  killAll();
  exit(3);

}

/* Kills all when signal is received*/
void signalHandler() {
  pid_t id = getpid();
  printf("Signal received... terminating child process %i\n", id);
  killAll();
  killpg(id, SIGINT);
  exit(EXIT_SUCCESS);
}

/* Release shared memory*/
void killAll() {
  shmdt(shm);
  shmdt(PBlock);
  fclose(fp);
}

int getIndex() {
  int c;
  for (c = 0; c < maxProc; c++) {
    if (PBlock[c] -> pid == getpid()) {
      return c;
    }
  }
}

