#include "oss.h"

const static unsigned long long int maxTimer = maxSectionlen * 1E9;

static char * queued = "Queued";
static char * interrupted = "interrupted";
static char * requeued = "Re-Queued";
static char * zero = "0";

static FILE * fp; // file pointer for log file

static int bitArray[maxProc] = {
  0
};
static int logfile_lines = 0;
static int lastMoved, TProcess, totalTimeSchedules, totalTime, numOfTimesLooped, totalWaitingTime = 0;
static int lastMoved, critical_msgid, receive_msgid, send_msgid, pcbId, shmId;

static struct msg_buf criticalMsgbuff, recieveMsgbuff, sendMsgbuff;
static struct PCB( * PBlock)[maxProc];
static struct ProcessQueue * Q1, * Q2, * Q3, * Q4;
static struct SHM * shm;
static unsigned long long int totalProcessCPUTime = 0;

int main(int argc, char * argv[]) {
  int child_count = 20; //no. of child processes
  int option;
  int seconds = 60;


  while ((option = getopt(argc, argv, "hf:s:n:w:")) != -1) {   // GetOpt options  
    switch (option) {
    case 'h':
      printf("This program takes the following possible arguments\n");
      printf("  -h  : to display this help message\n");
      printf("  -n x : to designate the max total number of child processes\n");
      printf("  -s x : to designate the number of child processes that should exist at one time\n");
      printf("NOTE : The default output log files for are OSSLogs.txt and logUSER.txt \n");
      return 0;
      break;
    case 's':
      seconds = atoi(optarg);
      break;
    case 'n':
      child_count = atoi(optarg);
      break;
    default:
      fprintf(stderr, "%s: ", argv[0]);
      perror("Invalid option. Use -h option to check the usage.");
      break;;
    }
  }

  signal(SIGINT, signalHandler);   // Signal Handler
  signal(SIGSEGV, signalHandler);


  srand((unsigned)(getpid() ^ time(NULL) ^ ((getpid()) >> maxProc)));   // Seed the random number generator


  const char * PATH = "./user";   // Logfile
  char * fileName = "OSSLogs.txt";

  fp = fopen(fileName, "a");
  if (fp == NULL) {
    printf("Couldn't open file");
    errno = ENOENT;
    killAll();
    exit(EXIT_FAILURE);
  }


  if ((shmId = shmget(key, sizeof(struct SHM * ) * 3, IPC_CREAT | 0666)) < 0) {   // Attach shared memory
    perror("shmget");
    fprintf(stderr, "shmget() returned an error! Program terminating...\n");
    exit(EXIT_FAILURE);
  }

  if ((shm = (struct SHM * ) shmat(shmId, NULL, 0)) == (struct SHM * ) - 1) {
    perror("shmat");
    fprintf(stderr, "shmat() returned an error! Program terminating...\n");
    exit(EXIT_FAILURE);
  }



  if ((pcbId = shmget(PCBKey, sizeof(struct PCB * ) * (maxProc * 100), IPC_CREAT | 0666)) < 0) {   // Attach Process Control Block
    perror("shmget");
    fprintf(stderr, "shmget() returned an error! Program terminating...\n");
    exit(EXIT_FAILURE);
  }

  if ((PBlock = (void * )(struct PCB * ) shmat(pcbId, NULL, 0)) == (void * ) - 1) {
    perror("shmat");
    fprintf(stderr, "shmat() 1 returned an error! Program terminating...\n");
    exit(EXIT_FAILURE);
  }

 
  shm -> turn = 0;  // Initialize the queues Q1, Q2 and Q3 and set everything to 0
  shm -> Schd_Cnt = 0;
  shm -> timePassedSec = 0;

  Q1 = malloc(sizeof( * Q1));
  Q1 -> numProc = 0;

  Q2 = malloc(sizeof( * Q2));
  Q2 -> numProc = 0;

  Q3 = malloc(sizeof( * Q3));
  Q3 -> numProc = 0;

  Q4 = malloc(sizeof( * Q4));
  Q4 -> numProc = 0;

  int c, status,
  index = 0;
  pid_t pid, temp, wpid;

  for (c = 0; c < maxProc; c++) {
    shm -> flag[c] = 0;
  }


  int msgflg = IPC_CREAT | 0666;   // Message passing variables


  if ((receive_msgid = msgget(OSSKey, msgflg)) < 0) {   // Messages - Sending And Receiving
    perror("msgget");
    killAll();
    exit(EXIT_FAILURE);
  }

  if ((send_msgid = msgget(UserKey, msgflg)) < 0) {
    perror("msgget");
    killAll();
    exit(EXIT_FAILURE);
  }

  if ((critical_msgid = msgget(CSKey, msgflg)) < 0) {
    perror("msgget");
    killAll();
    exit(EXIT_FAILURE);
  }


  pid = fork();   // Forking the child process and initiating the clock
  if (pid == 0) {
    execl("./clock", NULL);
  } else if (pid < 0) {
    printf("There was an error creating the clock\n");
    signalHandler();
  }
  usleep(5000);

  int ProcessSpawn = 0;
  int waitUntil, result;


  while (logfile_lines < 10000) {   // Generate OSS simulation and interrupt it if it writes 10,000 lines to the log file
    result = -1;


    if (!ProcessSpawn) {     // Determine time between child spawns
      waitUntil = (rand() % spawnRateSetting);
      if (waitUntil == 3) {
        waitUntil = 0;
      }
      waitUntil += shm -> timePassedSec;
      ProcessSpawn = 1;
    }

    if ((index < maxProc) && (ProcessSpawn == 1) && (shm -> timePassedSec >= waitUntil)) {     // Spawn child if allowed
      index++;
      result = createProcess(TProcess);
      ProcessSpawn = 0;
      TProcess++;

      pid = fork();
      if (pid == 0) {
        execl(PATH, NULL);


        _exit(EXIT_FAILURE);
      } else if (pid < 0) {         //Error while forking
        printf("Error forking\n");
      } else {

        PBlock[result] -> pid = pid;
        PBlock[result] -> index = result;
        printf("OSS: Generated Process with PID %i at time %03i.%09lu - PROCESS #%i \n", pid, shm -> timePassedSec, shm -> timeelapsedNano, TProcess);
        if (logfile_lines < 10000) {
          fprintf(fp, "OSS: Generated Process with PID %i at time %03i.%09lu - PROCESS #%i\n", pid, shm -> timePassedSec, shm -> timeelapsedNano, TProcess);
          logfile_lines++;
        }
      }
    } else {
      pid = -1;
    }
    usleep(sleepTime * 10000);


    if (result != -1 && pid != -1) {     // All a child to queue as soon as it spawned
      EnQueue(PBlock[result] -> queue, pid, queued);
    }


    scheduleProcess(index, result);     // Run the scheduler


    if (msgrcv(receive_msgid, & recieveMsgbuff, MessageBuffer, 1, IPC_NOWAIT) > 0) {     // If OSS receives a message from a child requesting control
      printf("OSS: Giving control to process with PID %s at time %03i.%09lu\n", recieveMsgbuff.mtext, shm -> timePassedSec, shm -> timeelapsedNano);
      if (logfile_lines < 10000) {
        fprintf(fp, "OSS: Giving control to process with PID %s at time %03i.%09lu\n", recieveMsgbuff.mtext, shm -> timePassedSec, shm -> timeelapsedNano);
        logfile_lines++;
      }

      shm -> childControl = 1;
      temp = atoi(recieveMsgbuff.mtext);

      usleep(sleepTime * 10000);

      criticalMsgbuff.mtype = 1;
      sprintf(criticalMsgbuff.mtext, "%i", temp);


      if (msgsnd(critical_msgid, & criticalMsgbuff, MessageBuffer, IPC_NOWAIT) < 0) {       // OSS saying child that it has control
        perror("msgsnd");
        printf("The reply to child did not send\n");
        signalHandler();
      }


      while (msgrcv(receive_msgid, & recieveMsgbuff, MessageBuffer, 0, 0) < 0);       // Taking control back from the child
      printf("OSS: Taking control back from process with PID %i at time %03i.%09lu\n", temp, shm -> timePassedSec, shm -> timeelapsedNano);
      if (logfile_lines < 10000) {
        fprintf(fp, "OSS: Taking control back from process with PID %i at time %03i.%09lu\n", temp, shm -> timePassedSec, shm -> timeelapsedNano);
        logfile_lines++;
      }

      usleep(sleepTime * 10000);
      shm -> childControl = 0;

      criticalMsgbuff.mtype = 1;
      sprintf(criticalMsgbuff.mtext, "%i", temp);

      if (msgsnd(critical_msgid, & criticalMsgbuff, MessageBuffer, IPC_NOWAIT) < 0) {
        perror("msgsnd");
        printf("The reply to child did not send\n");
        signalHandler();
      }


      if (atoi(recieveMsgbuff.mtext) == 1) {       // Insert the child to queue again if it didnot finish 
        if (recieveMsgbuff.mtype <= 5E8) {
          EnQueue(1, temp, requeued);
        } else if (recieveMsgbuff.mtype <= 1E9) {
          EnQueue(2, temp, requeued);
        } else {
          EnQueue(3, temp, requeued);
        }
      }
    }

    usleep(sleepTime * 10000);


    if (Q4 -> numProc > 5) {     // Clean the queue up
      Q4 -> pid[0] = 0;
      Q4 -> numProc--;
      advanceQueues();
    }


    if ((wpid = waitpid(-1, & status, WNOHANG)) > 0) {     // If a child finished, clear the PBlock and reset for a new child to spawn
      index--;
      for (c = 0; c < maxProc; c++) {
        if (PBlock[c] -> pid == wpid) {
          printf("OSS: Terminating process with PID %i at time %03i.%09lu\n", wpid, shm -> timePassedSec, shm -> timeelapsedNano);
          if (logfile_lines < 10000) {
            fprintf(fp, "OSS: Terminating process with PID %i at time %03i.%09lu\n", wpid, shm -> timePassedSec, shm -> timeelapsedNano);
            logfile_lines++;
          }

          bitArray[c] = 0;
          shm -> Schd_Cnt--;
          printProcessStats(c);
          EnQueue(4, wpid, queued);
          advanceQueues();
          break;
        }
      }
    }
    usleep(sleepTime * 40000);
    numOfTimesLooped++;
  }

  printf("***File has reached its size limit... waiting for all processes to terminate at time %03i.%09lu\n****", shm -> timePassedSec, shm -> timeelapsedNano);

  for (c = 0; c < Q4 -> numProc; c++) {
    Q4 -> pid[c] = 0;
  }

  killAll();
  printf("OSS: Exiting -- Killing all its child processes\n");
  printStats();
  killpg(getpgrp(), SIGINT);

  sleep(1);
  return 0;
}

void EnQueue(int queue, pid_t pid, char * word) {
  int c;

  int pos_index;
  for (pos_index = 0; pos_index < maxProc; pos_index++) {
    if (PBlock[pos_index] -> pid == Q1 -> pid[0]) {
      break;
    }
  }

  if (PBlock[pos_index] -> alive == 0) {
    PBlock[pos_index] -> alive = 1;
    PBlock[pos_index] -> inQueueSec = shm -> timePassedSec;
    PBlock[pos_index] -> inQueueNansec = shm -> timeelapsedNano;
  }
  switch (queue) {
  case 1:
    for (c = 0; c < maxProc; c++) {
      if (Q1 -> pid[c] == 0) {
        Q1 -> pid[c] = pid;
        Q1 -> index[c] = pos_index;
        usleep(10);
        printf("OSS: %s Process with PID %i to Queue #%i at time %03i.%09lu\n", word, pid, queue, shm -> timePassedSec, shm -> timeelapsedNano);
        if (logfile_lines < 10000) {
          fprintf(fp, "OSS: %s Process with PID %i to Queue #%i at time %03i.%09lu\n", word, pid, queue, shm -> timePassedSec, shm -> timeelapsedNano);
          logfile_lines++;
        }
        Q1 -> numProc++;
        break;
      }
    }

    break;

  case 2:
    for (c = 0; c < maxProc; c++) {
      if (Q2 -> pid[c] == 0) {
        Q2 -> pid[c] = pid;
        Q2 -> index[c] = pos_index;
        usleep(10);
        printf("OSS: %s Process with PID %i to Queue #%i at time %03i.%09lu\n", word, pid, queue, shm -> timePassedSec, shm -> timeelapsedNano);
        if (logfile_lines < 10000) {
          fprintf(fp, "OSS: %s Process with PID %i to Queue #%i at time %03i.%09lu\n", word, pid, queue, shm -> timePassedSec, shm -> timeelapsedNano);
          logfile_lines++;
        }
        Q2 -> numProc++;
        break;
      }
    }

    break;

  case 3:
    for (c = 0; c < maxProc; c++) {
      if (Q3 -> pid[c] == 0) {
        Q3 -> pid[c] = pid;
        Q3 -> index[c] = pos_index;
        usleep(10);
        printf("OSS: %s Process with PID %i to Queue #%i at time %03i.%09lu\n", word, pid, queue, shm -> timePassedSec, shm -> timeelapsedNano);
        if (logfile_lines < 10000) {
          fprintf(fp, "OSS: %s Process with PID %i to Queue #%i at time %03i.%09lu\n", word, pid, queue, shm -> timePassedSec, shm -> timeelapsedNano);
          logfile_lines++;
        }
        Q3 -> numProc++;
        break;
      }
    }

    break;

  case 4:
    for (c = 0; c < maxProc; c++) {
      if (Q4 -> pid[c] == 0) {
        Q4 -> pid[c] = pid;
        Q4 -> numProc++;
        break;
      }
    }

    break;

  default:
    printf("Error: Could not add child with PID %i to a queue!\n", pid);
    fprintf(stderr, "Error adding to queue\n");
    fprintf(fp, "Error: Could not add child with PID %i to a queue!\n", pid);
    break;
  }
}

void advanceQueues() {
  int c;
  int x;

  for (c = 0; c < maxProc; c++) {
    if (Q1 -> pid[c] == 0) {
      x = c + 1;
      if (x < maxProc) {
        if (Q1 -> pid[x] > 0) {
          Q1 -> pid[c] = Q1 -> pid[x];
          Q1 -> pid[x] = 0;
          Q1 -> index[c] = Q1 -> index[x];
          Q1 -> index[x] = 0;
        }
      }
    }
  }

  for (c = 0; c < maxProc; c++) {
    if (Q2 -> pid[c] == 0) {
      x = c + 1;
      if (x < maxProc) {
        if (Q2 -> pid[x] > 0) {
          Q2 -> pid[c] = Q2 -> pid[x];
          Q2 -> pid[x] = 0;
          Q2 -> index[c] = Q2 -> index[x];
          Q2 -> index[x] = 0;
        }
      }
    }
  }

  for (c = 0; c < maxProc; c++) {
    if (Q3 -> pid[c] == 0) {
      x = c + 1;
      if (x < maxProc) {
        if (Q3 -> pid[x] > 0) {
          Q3 -> pid[c] = Q3 -> pid[x];
          Q3 -> pid[x] = 0;
          Q3 -> index[c] = Q3 -> index[x];
          Q3 -> index[x] = 0;
        }
      }
    }
  }

  for (c = 0; c < maxProc; c++) {
    if (Q4 -> pid[c] == 0) {
      x = c + 1;
      if (x < maxProc) {
        if (Q4 -> pid[x] > 0) {
          Q4 -> pid[c] = Q4 -> pid[x];
          Q4 -> pid[x] = 0;
          Q4 -> index[c] = Q4 -> index[x];
          Q4 -> index[x] = 0;
        }
      }
    }
  }
}

int createProcess(int ProcessNum) {
  int indx;

  for (indx = 0; indx < maxProc; indx++) {
    if (bitArray[indx] == 0) {
      bitArray[indx] = 1;
      break;
    }
  }

  if (indx == maxProc) {
    return -1;
  }

  PBlock[indx] -> index = indx;
  PBlock[indx] -> pid = 0;
  PBlock[indx] -> ProcessNum = ProcessNum;
  PBlock[indx] -> cpuTime = PBlock[indx] -> Quantum = rand() % maxTimer + 1;
  PBlock[indx] -> queue = 0;
  PBlock[indx] -> creationSec = shm -> timePassedSec;
  PBlock[indx] -> creationNansec = shm -> timeelapsedNano;
  PBlock[indx] -> finishSec = 0;
  PBlock[indx] -> inQueueSec = 0;
  PBlock[indx] -> inQueueNansec = 0;
  PBlock[indx] -> moveFlag = 0;

  if (PBlock[indx] -> Quantum <= 500000000) {
    PBlock[indx] -> queue = 1;
  } else if (PBlock[indx] -> Quantum <= 1000000000) {
    PBlock[indx] -> queue = 2;
  } else {
    PBlock[indx] -> queue = 3;
  }

  int num = rand() % 101;
  if (prioritySetting > num) {
    PBlock[indx] -> queue = 1;
  }

  return indx;
}

void printProcessStats(int indx) {
  unsigned long long int temp = shm -> timeelapsedNano - PBlock[indx] -> creationNansec;
  if (temp < 0) {
    temp = (shm -> timeelapsedNano + PBlock[indx] -> creationNansec) - 1E9;
  }

  totalTime += (PBlock[indx] -> finishSec - PBlock[indx] -> creationSec);
  totalProcessCPUTime += PBlock[indx] -> cpuTime;
  printf("-----------------------------------------------------------------------------------\n");
  printf("----------Process with PID %i was in the system for time %i.%.9u seconds-----------------\n", PBlock[indx] -> pid, (PBlock[indx] -> finishSec - PBlock[indx] -> creationSec), temp / 1E9);
  printf("--------------Process %i used %f seconds of CPU time-----------------------\n", PBlock[indx] -> pid, (double)(PBlock[indx] -> cpuTime / 1E9));
  printf("-----------------------------------------------------------------------------------\n");

  if (logfile_lines < 9994) {
    fprintf(fp, "-----------------------------------------------------------------------------------\n");
    fprintf(fp, "----------Process with PID %i was in the system for time %i.%.9u seconds-----------------\n", PBlock[indx] -> pid, (PBlock[indx] -> finishSec - PBlock[indx] -> creationSec), temp / 1E9);
    fprintf(fp, "--------------Process wih PID %i used %f seconds of CPU time----------------------\n", PBlock[indx] -> pid, (double)(PBlock[indx] -> cpuTime / 1E9));
    fprintf(fp, "-----------------------------------------------------------------------------------\n");
    logfile_lines += 4;
  }

  PBlock[indx] -> alive = 0;
  PBlock[indx] -> index = 0;
  PBlock[indx] -> pid = 0;
  PBlock[indx] -> ProcessNum = 0;
  PBlock[indx] -> cpuTime = PBlock[indx] -> Quantum = 0;
  PBlock[indx] -> queue = 0;
  PBlock[indx] -> creationSec = 0;
  PBlock[indx] -> creationNansec = 0;
  PBlock[indx] -> finishSec = 0;
  PBlock[indx] -> inQueueSec = 0;
  PBlock[indx] -> inQueueNansec = 0;
  PBlock[indx] -> moveFlag = 0;
}

void killAll() {
  msgctl(send_msgid, IPC_RMID, NULL);
  msgctl(receive_msgid, IPC_RMID, NULL);
  msgctl(critical_msgid, IPC_RMID, NULL);

  shmdt(shm);
  shmctl(shmId, IPC_RMID, NULL);

  shmdt(PBlock);
  shmctl(pcbId, IPC_RMID, NULL);

  free(Q1);
  free(Q2);
  free(Q3);
  free(Q4);

  fclose(fp);
}

void printQueues() {
  int c;
  for (c = 0; c < maxProc; c++) {
    printf("Q1 SLOT %i -- %i\n", c, Q1 -> pid[c]);
  }

  for (c = 0; c < maxProc; c++) {
    printf("Q2 SLOT %i -- %i\n", c, Q2 -> pid[c]);
  }

  for (c = 0; c < maxProc; c++) {
    printf("Q3 SLOT %i -- %i\n", c, Q3 -> pid[c]);
  }

  for (c = 0; c < maxProc; c++) {
    printf("Q4 SLOT %i -- %i\n", c, Q4 -> pid[c]);
  }
  printf("----------------------\n");
}

void printStats() {
  sleep(1);
  printf("-----------------------------------------------------------------------------------\n");
  printf("\t\tAverage Wait Time in Queue: %f seconds\n", ((double) totalWaitingTime / (double) totalTimeSchedules));
  printf("\t\tAverage CPU Usage by Processes: %f seconds\n", ((double)(totalProcessCPUTime / 1E9) / (double) TProcess));
  printf("\t\tAverage Turnaround Time: %f seconds\n", ((double) totalTime / (double) TProcess));
  printf("\t\tTotal Program Run Time: %i.%09lu seconds\n", shm -> timePassedSec, shm -> timeelapsedNano);
  printf("\t\tTotal CPU Down Time: %f seconds\n", ((double) shm -> timePassedSec - ((((double)(sleepTime + 2) * 1e-2) * numOfTimesLooped) + (double)(totalProcessCPUTime / 1E9))));
  printf("\t\tTotal Lines Printed: %i\n", logfile_lines);
  printf("-----------------------------------------------------------------------------------\n");

  fprintf(fp, "-----------------------------------------------------------------------------------\n");
  fprintf(fp, "\t\tAverage Wait Time in Queue: %f seconds\n", ((double) totalWaitingTime / (double) totalTimeSchedules));
  fprintf(fp, "\t\tAverage CPU Usage by Processes: %f seconds\n", ((double)(totalProcessCPUTime / 1E9) / (double) TProcess));
  fprintf(fp, "\t\tAverage Turnaround Time: %f seconds\n", ((double) totalTime / (double) TProcess));
  fprintf(fp, "\t\tTotal Program Run Time: %i.%09lu seconds\n", shm -> timePassedSec, shm -> timeelapsedNano);
  fprintf(fp, "\t\tTotal CPU Down Time: %f seconds\n", (shm -> timePassedSec - ((((sleepTime + 2) * 1e-2) * numOfTimesLooped) + (double)(totalProcessCPUTime / 1E9))));
  fprintf(fp, "\t\tTotal Lines Printed: %i\n", logfile_lines);
  fprintf(fp, "-----------------------------------------------------------------------------------\n");
  sleep(2);
}

int DeQueue(int queue, pid_t pid) {
  int c, tmp;

  switch (queue) {
  case 1:
    for (c = 0; c < maxProc; c++) {
      if (Q1 -> pid[c] == pid) {
        tmp = Q1 -> index[c];
        unsigned long long int temp = shm -> timeelapsedNano - PBlock[tmp] -> inQueueNansec;
        if (temp < 0) {
          temp = (shm -> timeelapsedNano + PBlock[tmp] -> inQueueNansec) - 1E9;
        }

        totalWaitingTime += (shm -> timePassedSec - PBlock[tmp] -> inQueueSec);
        totalTimeSchedules++;
        printf("OSS: Process with PID %i was in queue for %03i.%09lu seconds\n", Q1 -> pid[c], (shm -> timePassedSec - PBlock[tmp] -> inQueueSec), temp);
        if (logfile_lines < 10000) {
          fprintf(fp, "OSS: Process with PID %i was in queue for %03i.%09lu seconds\n", Q1 -> pid[c], (shm -> timePassedSec - PBlock[tmp] -> inQueueSec), temp);
          logfile_lines++;
        }
        PBlock[tmp] -> inQueueSec = PBlock[tmp] -> inQueueNansec = shm -> timePassedSec;
        PBlock[tmp] -> moveFlag = 0;
        PBlock[tmp] -> queue = 0;
        Q1 -> pid[c] = 0;
        Q1 -> index[c] = 0;
        Q1 -> numProc--;
        advanceQueues();
        return tmp;
      }
    }
    break;

  case 2:
    for (c = 0; c < maxProc; c++) {
      if (Q2 -> pid[c] == pid) {
        tmp = Q2 -> index[c];

        Q2 -> pid[c] = 0;
        Q2 -> index[c] = 0;
        Q2 -> numProc--;
        advanceQueues();
        return tmp;
      }
    }
    break;

  case 3:
    for (c = 0; c < maxProc; c++) {
      if (Q3 -> pid[c] == pid) {
        tmp = Q3 -> index[c];

        Q3 -> pid[c] = 0;
        Q3 -> index[c] = 0;
        Q3 -> numProc--;
        advanceQueues();
        return tmp;
      }
    }
    break;

  default:
    printf("You're not supposed to be here...\n");
    advanceQueues();
    return -1;
  }
}

void scheduleProcess() {
  int c;
  usleep(sleepTime * 10000);
  for (c = 0; c < Q3 -> numProc; c++) {
    if (((shm -> timePassedSec - PBlock[Q3 -> index[c]] -> inQueueSec) > 2) && (PBlock[Q3 -> index[c]] -> moveFlag == 0) && (PBlock[Q3 -> index[c]] -> alive == 1) && (lastMoved = 0)) {
      PBlock[Q3 -> index[c]] -> queue = 2;
      PBlock[Q3 -> index[c]] -> moveFlag = 1;
      lastMoved++;

      usleep(sleepTime * 10000);

      EnQueue(2, Q3 -> pid[c], "Moved Starving");
      DeQueue(3, Q3 -> pid[c]);
      return;
    }
  }

  for (c = 0; c < Q2 -> numProc; c++) {
    if (((shm -> timePassedSec - PBlock[Q2 -> index[c]] -> inQueueSec) > 2) && (PBlock[Q2 -> index[c]] -> moveFlag == 0) && (PBlock[Q2 -> index[c]] -> alive == 1) && (lastMoved = 0)) {
      PBlock[Q2 -> index[c]] -> queue = 2;
      PBlock[Q2 -> index[c]] -> moveFlag = 1;
      lastMoved++;

      usleep(sleepTime * 10000);

      EnQueue(2, Q2 -> pid[c], "Moved Starving");
      DeQueue(3, Q2 -> pid[c]);
      return;
    }
  }

  if (lastMoved == 2) {
    lastMoved = 0;
  } else {
    lastMoved++;
  }

  if (Q1 -> pid[0] > 0) {
    shm -> Schd_Cnt++;

    sendMsgbuff.mtype = Q1 -> pid[0];
    sprintf(sendMsgbuff.mtext, "%i", shm -> Schd_Cnt);
    printf("OSS: Scheduled Process with PID %i at time %03i.%09lu\n", sendMsgbuff.mtype, shm -> timePassedSec, shm -> timeelapsedNano);
    if (logfile_lines < 10000) {
      fprintf(fp, "OSS: Scheduled Process with PID %i at time %03i.%09lu\n", sendMsgbuff.mtype, shm -> timePassedSec, shm -> timeelapsedNano);
      logfile_lines++;
    }

    if (msgsnd(send_msgid, & sendMsgbuff, MessageBuffer, IPC_NOWAIT) < 0) {
      perror("msgsnd");
      printf("The reply to child did not send\n");
      signalHandler();
    }

    DeQueue(1, Q1 -> pid[0]);
    PBlock[Q1 -> index[0]] -> queue = 0;

    usleep(sleepTime * 10000);
    return;

  } else if (Q2 -> pid[0] > 0) {
    EnQueue(1, Q2 -> pid[0], interrupted);
    PBlock[Q1 -> index[0]] -> queue = 1;
    DeQueue(2, Q2 -> pid[0]);

    usleep(sleepTime * 10000);
    return;

  } else if (Q3 -> pid[0] > 0) {
    EnQueue(2, Q3 -> pid[0], interrupted);
    PBlock[Q1 -> index[0]] -> queue = 2;
    DeQueue(3, Q3 -> pid[0]);

    usleep(sleepTime * 10000);
    return;
  }
}

void signalHandler() {
  printStats();
  killAll();
  pid_t id = getpgrp();
  killpg(id, SIGINT);
  printf("Signal received... terminating\n");
  sleep(1);
  exit(EXIT_SUCCESS);
}

