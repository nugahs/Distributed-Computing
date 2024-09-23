#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
typedef std::chrono::time_point<std::chrono::system_clock> Time;

enum messageType{
  PRIVILEGE,
  REQUEST,
  TERMINATE
};

typedef struct Message{
  enum messageType type;
  int senderID;
  int reqProcess;
} Message1;

#define MAX_NODES 30
#define FALSE 0
#define TRUE 1
#define NIL_PROCESS -1

int startPort;
std::atomic<int> totalReceivedMessages = ATOMIC_VAR_INIT(0);
std::atomic<long long int> totalResponseTime = ATOMIC_VAR_INIT(0);

int createReceiveSocket(int nodeID, int sockPORT){
  printf(" Node %d: createReceiveSocket -> Creating Socket %hu\n", nodeID, sockPORT);
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(sockPORT);
  bzero(&server.sin_zero, 0);

  int sockfd;
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
      printf(" Node %d: createReceiveSocket -> Error in creating socket :: Reason %s\n", nodeID, strerror(errno));
      exit(1);
  }

  if (bind(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0)
  {
      printf(" Node %d: createReceiveSocket -> Error in binding socket to %hu :: Reason %s\n", nodeID, ntohs(server.sin_port), strerror(errno));
      exit(1);
  }

  if (listen(sockfd, MAX_NODES))
  {
      printf(" Node %d: createReceiveSocket -> Error in listening to %hu :: Reason %s\n", nodeID, ntohs(server.sin_port), strerror(errno));
      exit(1);
  }

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100000;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
  {
      printf(" Node %d: createReceiveSocket -> Error in creating timeout for TCP connection :: Reason %s\n", nodeID, strerror(errno));
      exit(1);
  }
  return sockfd;
}

bool sendMessage(int nodeID, int destNodeID, messageType type, int needsReqProcess)
{
    assert(nodeID != destNodeID);

    Message1 message;
    message.senderID = nodeID;
    message.type = type;
    if(needsReqProcess != -1)
      message.reqProcess = needsReqProcess;

    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf(" Node %d: sendMessage -> Error in creating send socket :: Reason - %s\n", nodeID, strerror(errno));
        close(sockfd);
        return false;
    }

    struct sockaddr_in server;
    const char* myaddr = "127.0.0.1";
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(myaddr);
    server.sin_port = htons(startPort + destNodeID);
    bzero(&server.sin_zero, 8);

    if (connect(sockfd, (struct sockaddr *)(&server), sizeof(struct sockaddr_in)) < 0)
    {
        printf(" Node %d: sendMessage -> Error in connecting to server %d:: Reason - %s\n", nodeID, destNodeID, strerror(errno));
        close(sockfd);
        return false;
    }

    if (send(sockfd, (char *)(&message), sizeof(message), 0) < 0)
    {
        printf("WARN :: Node %d: sendMessage -> Error in sending message-type %s to %d :: Reason - %s\n", nodeID, ((type == PRIVILEGE) ? "PRIVILEGE" : "REQUEST"), destNodeID, strerror(errno));
        close(sockfd);
        return false;
    }

    close(sockfd);
    return true;
}

bool wantToEnter(int nodeID, int &does_request_cs, int &next_process, int &node_father, int &is_token_present, FILE *outfile, pthread_mutex_t *m_sendrec, Time &start){
  pthread_mutex_lock(m_sendrec);
  does_request_cs = TRUE;
  if(node_father != NIL_PROCESS){

    long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
    fprintf(outfile,"p%d sending request message to its node_father:p%d at %lld\n",nodeID,node_father,sysTime);
    fflush(outfile);
    if(sendMessage(nodeID, node_father, REQUEST, nodeID) == false) {
        printf(" Node %d: assignPrivilege -> Unable to send REQUEST to %d\n", nodeID, node_father);
        exit(1);
    }
    node_father = NIL_PROCESS;
  }
  pthread_mutex_unlock(m_sendrec);

  while(true){
    pthread_mutex_lock(m_sendrec);
    if(is_token_present){
        pthread_mutex_unlock(m_sendrec);
        break;
    }
    pthread_mutex_unlock(m_sendrec);
  }
}

bool wantToLeave(int nodeID, int &does_request_cs, int &next_process, int &node_father, int &is_token_present, FILE *outfile, pthread_mutex_t *m_sendrec, Time &start){
  pthread_mutex_lock(m_sendrec);

  does_request_cs = FALSE;
  if(next_process != NIL_PROCESS){
    long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
    fprintf(outfile,"p%d sending its token to its next_process:p%d at %lld\n",nodeID,next_process,sysTime);
    fflush(outfile);
    if(sendMessage(nodeID, next_process, PRIVILEGE,-1) == false) {
        printf(" Node %d: assignPrivilege -> Unable to send PRIVILEGE to %d\n", nodeID, node_father);
        exit(1);
    }
    is_token_present = FALSE;
    next_process = NIL_PROCESS;
  }
  pthread_mutex_unlock(m_sendrec);

}

void do_work(int nodeID, int &node_father, int &next_process, int &does_request_cs, int &is_token_present, int noOfNodes, int cnt_of_mutex,
          std::atomic<int> &Cnt_EndedProcesses, double lambda1, double lambda2, Time &start, FILE *outfile, pthread_mutex_t *m_sendrec){
  std::default_random_engine generator1;
  std::exponential_distribution<double> distribution1(1/lambda1);

  std::default_random_engine generator2;
  std::exponential_distribution<double> distribution2(1/lambda2);

  long long int sysTime;
  Time requestCSTime;

  printf(" Node %d: working -> Starting Critical Section Simulations\n", nodeID);

  for(int i = 1; i<=cnt_of_mutex; i++){
      int outCSTime = distribution1(generator1);
      int inCSTime = distribution2(generator2);

      sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d request to enter CS at %lld for the %dth time\n",nodeID,sysTime,i);
      fflush(outfile);
      sleep(outCSTime);
      requestCSTime = std::chrono::system_clock::now();
      wantToEnter(nodeID,does_request_cs,next_process,node_father, is_token_present, outfile,m_sendrec,start);
      totalResponseTime += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - requestCSTime).count();;

      sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d enters CS at %lld\n",nodeID,sysTime);
      fflush(outfile);

      sleep(inCSTime);

      sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d is doing local computation at %lld\n",nodeID,sysTime);
      fflush(outfile);

      sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d leaves CS at %lld\n",nodeID,sysTime);
      fflush(outfile);
      wantToLeave(nodeID,does_request_cs,next_process,node_father,is_token_present,outfile,m_sendrec,start);
  }

  sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
  fprintf(outfile, "%d completed all %d transactions at %lld\n", nodeID, cnt_of_mutex, sysTime);
  fflush(outfile);

  for(int i = 0;i<noOfNodes;i++){
    if(i != nodeID){
      sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d sends terminate message to %d at %lld\n",nodeID,i,sysTime);
      fflush(outfile);
      if (sendMessage(nodeID, i, TERMINATE,-1) == false)
      {
          printf(" Node %d: working -> Could not send TERMINATE to %d\n", nodeID, i);
          exit(EXIT_FAILURE);
      }
    }
  }

  pthread_mutex_lock(m_sendrec);
  Cnt_EndedProcesses += 1;
  pthread_mutex_unlock(m_sendrec);

  while(Cnt_EndedProcesses < noOfNodes);
  sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
  fprintf(outfile, "%d finished any pending transactions at %lld\n", nodeID, sysTime);
  fflush(outfile);
}

void do_receive(int nodeID, int sockPORT, int noOfNodes, int &node_father, int &next_process, int &does_request_cs, int &is_token_present,
             std::atomic<int> &Cnt_EndedProcesses, Time &start, FILE *outfile, pthread_mutex_t *m_sendrec){

  struct sockaddr_in client;
  socklen_t len = sizeof(struct sockaddr_in);

  Message1 message;
  Time now;
  int clientId;
  int send_process;
  long long int sysTime;
  double actionEnterTime;

  int sockfd = createReceiveSocket(nodeID, sockPORT);
  printf(" Node %d: receiveMessage -> Started listening for connections\n", nodeID);

  while(Cnt_EndedProcesses < noOfNodes){

    if ((clientId = accept(sockfd, (struct sockaddr *)&client, &len)) >= 0){
      int data_len = recv(clientId, (char *)&message, sizeof(message), 0);
      if (data_len > 0) {
        now = std::chrono::system_clock::now();
        totalReceivedMessages++;
        if (message.type == PRIVILEGE) {
            sysTime = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
            fprintf(outfile, "p%d receives p%d\'s token to enter CS at %lld\n", nodeID, message.senderID, sysTime);
            fflush(outfile);
            pthread_mutex_lock(m_sendrec);
            is_token_present = TRUE;
            pthread_mutex_unlock(m_sendrec);
        } else if (message.type == REQUEST) {
            send_process = message.reqProcess;
            sysTime = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
            fprintf(outfile, "p%d receives p%d\'s request to enter CS at %lld\n", nodeID, send_process, sysTime);
            fflush(outfile);
            pthread_mutex_lock(m_sendrec);
            if (node_father == NIL_PROCESS) {
                fprintf(outfile, "p%d has the token\n", nodeID);
                fflush(outfile);
                if (does_request_cs) {
                    fprintf(outfile, "p%d is currently requesting for the token\n", nodeID);
                    fflush(outfile);
                    next_process = send_process;
                } else {
                    is_token_present = FALSE;
                    sysTime = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
                    fprintf(outfile, "p%d sending token message to the requested process:p%d at %lld\n", nodeID, send_process, sysTime);
                    fflush(outfile);
                    if (sendMessage(nodeID, send_process, PRIVILEGE, -1) == false) {
                        printf(" Node %d: working -> Could not send PRIVILEDGE to %d\n", nodeID, send_process);
                        exit(EXIT_FAILURE);
                    }
                }
            } else {
                sysTime = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
                fprintf(outfile, "p%d forwarding request message to its node_father:p%d at %lld\n", nodeID, node_father, sysTime);
                fflush(outfile);
                if (sendMessage(nodeID, node_father, REQUEST, send_process) == false) {
                    printf(" Node %d: working -> Could not send REQUEST to %d\n", nodeID, node_father);
                    exit(EXIT_FAILURE);
                }
            }
            node_father = message.reqProcess;
            pthread_mutex_unlock(m_sendrec);
        } else if (message.type == TERMINATE) {
            sysTime = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
            fprintf(outfile, "p%d receives terminate message from %d at %lld\n", nodeID, message.senderID, sysTime);
            fflush(outfile);
            pthread_mutex_lock(m_sendrec);
            Cnt_EndedProcesses += 1;
            pthread_mutex_unlock(m_sendrec);
        } else {
            printf(" Node %d: receiveMessage -> Invalid Message Type %d\n", nodeID, message.type);
        }
      }
    }
  }
  sysTime = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
  fprintf(outfile, "%d stopped receiving threads at %lld\n", nodeID, sysTime);
  fflush(outfile);
}

void do_run(int noOfNodes, int mutualExclusionCnt, int FirstTokenNode, double alpha, double beta)
{
    fclose(fopen("output.txt", "w"));
    FILE *fp = fopen("output.txt", "a+");
    FILE *file_pointers[noOfNodes];
    std::vector<pthread_mutex_t> locks(noOfNodes);

    for(int i  = 0;i<noOfNodes; i++){
      pthread_mutex_init(&locks[i],NULL);
    }
    Time start = std::chrono::system_clock::now();
    std::vector<std::thread> workerSenders(noOfNodes);
    std::vector<std::thread> workerReceivers(noOfNodes);

    std::vector<int> node_father(noOfNodes, FirstTokenNode);
    node_father[FirstTokenNode] = NIL_PROCESS;
    std::vector<int> next_process(noOfNodes, NIL_PROCESS);
    std::vector<int> does_request_cs(noOfNodes, FALSE);
    std::vector<int> is_token_present(noOfNodes, FALSE);
    is_token_present[FirstTokenNode] = TRUE;
    std::vector<std::atomic<int>> Cnt_EndedProcesses(noOfNodes);

    printf("Creating receiver threads\n");

    for (int i = 0; i < noOfNodes; i++)
    {
        Cnt_EndedProcesses[i] = ATOMIC_VAR_INIT(0);

        workerReceivers[i] = std::thread(do_receive, i, startPort + i, noOfNodes,
            std::ref(node_father[i]), std::ref(next_process[i]), std::ref(does_request_cs[i]), std::ref(is_token_present[i]),
            ref(Cnt_EndedProcesses[i]), ref(start), fp, &locks[i]);
    }
    usleep(1000000);
    std::cout << "Creating CS executor threads" << std::endl;

    for (int i = 0; i < noOfNodes; i++)
    {
        workerSenders[i] = std::thread(do_work, i,
            std::ref(node_father[i]), std::ref(next_process[i]), std::ref(does_request_cs[i]), std::ref(is_token_present[i]), noOfNodes,
            mutualExclusionCnt, std::ref(Cnt_EndedProcesses[i]), alpha, beta, std::ref(start), fp, &locks[i]);
    }

    for (int i = 0; i < noOfNodes; i++)
    {
        workerSenders[i].join();
        workerReceivers[i].join();
    }

    float averageMessagesExchanged = totalReceivedMessages / (noOfNodes*1.0*mutualExclusionCnt);
    double averageResponseTime = totalResponseTime / (1000.0 * noOfNodes * mutualExclusionCnt);

    std::cout << "\n\nAnalysis:\n\tTotal Messages Exchanged:  " << totalReceivedMessages << "\n\tAverage Messages Exchanged: " << averageMessagesExchanged << std::endl;
    std::cout <<"\n\tAverage Response Time: " << averageResponseTime << " milliseconds" << std::endl;

    for(int i  = 0;i<noOfNodes; i++)
      pthread_mutex_destroy(&locks[i]);

}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "\033[1;31mMissing input file path in arguments\033[0m\n";
        exit(EXIT_FAILURE);
    }
    std::ifstream fin(argv[1]);

    if (!fin)
    {
        std::cout << "\033[1;31mError In Opening inp-params.txt\033[0m\n";
        exit(EXIT_FAILURE);
    }
    if (argc < 3)
      startPort = 10000;
    else
      startPort = atoi(argv[2]);

    srand(time(NULL));

    int noOfNodes,
        mutualExclusionCnt,
        FirstTokenNode;
    double alpha,
         beta;

    fin >> noOfNodes >> mutualExclusionCnt >> FirstTokenNode >> alpha >> beta;
    fin.close();
    do_run(noOfNodes, mutualExclusionCnt, FirstTokenNode, alpha, beta);

    return 0;
}