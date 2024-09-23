#include <map>
#include <mutex>
#include <algorithm>
#include <set>
#include <vector>
#include <sstream>
#include <assert.h> 
#include <iostream>
#include <string.h>
#include <arpa/inet.h>
#include <list>
#include <chrono>

#define MAX_NODES 50
#define MAX_LENGTH 500
#define FALSE 0
#define TRUE 1
int startPort;

typedef std::chrono::time_point<std::chrono::system_clock> Time;
typedef long long int LLONG;

enum messageType
{
    TOKEN,
    REQUEST,
    TERMINATE
};

typedef struct RequestMessage
{
    enum messageType type;
    int senderID;      
    int reqOriginId;    

    LLONG reqTime;

    char alreadySeen[MAX_LENGTH];

} RequestMessage;

typedef struct Token
{
    enum messageType type;
    int senderID;              

    int elecID;              

    LLONG lud[MAX_NODES];
} Token;

typedef struct TerminateMessage
{
    enum messageType type;
    int senderID;
} TerminateMessage;

typedef struct RequestID
{
    int reqOriginId;
    LLONG reqTime;
} RequestID;

typedef std::list<RequestID> RequestArrayNode;

int createReceiveSocket(const int myPID, const int myPort)
{
    std::cout << "INFO :: Node " << myPID << ": createReceiveSocket -> Creating Socket " << myPort << std::endl;
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(myPort);
    bzero(&server.sin_zero, 0);

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        std::cout << "ERROR :: Node " << myPID << " createReceiveSocket -> Error in creating socket :: Reason " <<  strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0)
    {
        std::cout << "ERROR :: Node " << myPID << " createReceiveSocket -> Error in creating socket to "  << ntohs(server.sin_port) << " :: Reason " <<  strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, MAX_NODES))
    {
        std::cout << "ERROR :: Node " << myPID << " createReceiveSocket -> Error in listening to "  << ntohs(server.sin_port) << " :: Reason " <<  strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        std::cout << "ERROR :: Node " << myPID << ": createReceiveSocket -> Error in creating timeout for TCP connection :: Reason " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

template <typename T>
bool sendMessage(const int myPID, const int dstID, const T &message)
{
    assert(myPID != dstID);

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("ERROR :: Node %d: sendMessage -> Error in creating send socket :: Reason - %s\n", myPID, strerror(errno));
        close(sockfd);
        return false;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(startPort + dstID);
    bzero(&server.sin_zero, 8);

    if (connect(sockfd, (struct sockaddr *)(&server), sizeof(struct sockaddr_in)) < 0)
    {
        printf("ERROR :: Node %d: sendMessage -> Error in connecting to server %d:: Reason - %s\n", myPID, dstID, strerror(errno));
        close(sockfd);
        return false;
    }

    if (send(sockfd, (char *)(&message), sizeof(message), 0) < 0)
    {
        printf("WARN :: Node %d: sendMessage -> Error in sending message-type %s to %d :: Reason - %s\n", myPID, ((message.type == TOKEN) ? "TOKEN" : ((message.type == REQUEST) ? "REQUEST" : "TERMINATE")), dstID, strerror(errno));
        close(sockfd);
        return false;
    }

    close(sockfd);
    return true;
}

int getNeighborIDfromReqArray(const int myPID, const int elecID, std::map<int, RequestArrayNode> &reqArray)
{
    for (auto &neighbor : reqArray)
    {
        for (auto iter = neighbor.second.begin(); iter != neighbor.second.end(); iter++)
        {
            if (iter->reqOriginId == elecID)
                return neighbor.first;
        }
    }

    printf("ERROR :: Node %d: getNeighborIDfromReqArray -> elecID not found in request array\n", myPID);
    exit(EXIT_FAILURE);
}

std::string getLudFromToken(const int numNodes, Token **sharedTokenPtrPtr)
{
    std::string ans = "";
    for (int i = 0; i < numNodes; i++)
        ans += std::to_string((*sharedTokenPtrPtr)->lud[i]) + ",";
    int ans_sz = ans.size();
    if(ans_sz > 0)
        return ans.substr(0, ans_sz - 1);
    return ans; 
}

std::vector<int> extractProcessIDFromAlreadySeen(char Q[])
{
    std::string str = std::string(Q);
    std::vector<int> pid;
    std::stringstream ss(str);
    int i;
    while (ss >> i)
    {
        pid.push_back(i);
        if (ss.peek() == ',')
            ss.ignore();
    }
    return pid;
}

std::string unionAlreadySeenNeighbors(std::vector<int> alreadySeen, std::vector<int> &neighbors)
{
    std::set<int> myset;
    for (auto i : alreadySeen)
        myset.insert(i);
    for (auto i : neighbors)
        myset.insert(i);

    std::string ans = "";
    for (auto neighbor : myset)
        ans += std::to_string(neighbor) + ",";

    int ans_sz = ans.size();
    if(ans_sz > 0)
        return ans.substr(0, ans.size() - 1);
    return ans;
}

std::string constructAlreadySeenString(const int myPID, std::vector<int> &neighbors)
{
    std::string s = "";
    for (const int neighbor : neighbors)
        s += std::to_string(neighbor) + ",";
    s += std::to_string(myPID);
    return s;
}

void transmitToken(const int myPID, int &tokenHere, LLONG &logicalClock, std::map<int, RequestArrayNode> &reqArray,
                   Token **ptrToPtrOfSharedTokens, const int numNodes, const Time &start, FILE *fp)
{
    if (*ptrToPtrOfSharedTokens == NULL)
    {
        std::cout << "WARN :: Node " << myPID << ": transmitToken -> ptrToPtrOfSharedTokens points to NULL" << std::endl;
        return;
    }
    LLONG smallestReqTime = 0;
    int smallestReqID = -1;
    int smallestNbrVal;
    std::list<RequestID>::iterator smallestReqNodeIter;

    for (auto &num : reqArray)
    {
        for (auto iter = num.second.begin(); iter != num.second.end(); iter++)
        {
            if ((*ptrToPtrOfSharedTokens)->lud[iter->reqOriginId] < 0 || (*ptrToPtrOfSharedTokens)->lud[iter->reqOriginId] < iter->reqTime)
            {
                if (smallestReqID != -1)
                {
                    if (iter->reqTime < smallestReqTime)
                    {
                        smallestReqTime = iter->reqTime;
                        smallestReqID = iter->reqOriginId;
                        smallestNbrVal = num.first;
                        smallestReqNodeIter = iter;
                    }
                    else if (iter->reqTime == smallestReqTime)
                    {
                        if (iter->reqOriginId < smallestReqID)
                        {
                            smallestReqTime = iter->reqTime;
                            smallestReqID = iter->reqOriginId;
                            smallestNbrVal = num.first;
                            smallestReqNodeIter = iter;
                        }
                    }
                }
                else
                {
                    smallestReqTime = iter->reqTime;
                    smallestReqID = iter->reqOriginId;
                    smallestNbrVal = num.first;
                    smallestReqNodeIter = iter;
                }
            }
        }
    }

    if (smallestReqID != -1)
    {
        reqArray[smallestNbrVal].erase(smallestReqNodeIter);

        (*ptrToPtrOfSharedTokens)->type = messageType::TOKEN;
        (*ptrToPtrOfSharedTokens)->senderID = myPID;
        (*ptrToPtrOfSharedTokens)->elecID = smallestReqID;
        (*ptrToPtrOfSharedTokens)->lud[myPID] = logicalClock;

        logicalClock++;
        tokenHere = false;
        std::cout << "Node " << myPID << ": transmitToken -> Sending Token for " << smallestReqID << " with clock " << smallestReqTime << " to neighbor " << smallestNbrVal << std::endl;
        long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
        fprintf(fp, "%d sends TOKEN to neighbor %d for %d with lud |%s| at %lld\n", myPID, smallestNbrVal, smallestReqID, getLudFromToken(numNodes, ptrToPtrOfSharedTokens).c_str(), sysTime);
        fflush(fp);

        if (sendMessage(myPID, smallestNbrVal, **ptrToPtrOfSharedTokens) == false)
        {
            std::cout << "ERROR :: Node " << myPID << ": transmitToken -> Unable to send TOKEN to " << smallestNbrVal << std::endl;
            exit(EXIT_FAILURE);
        }
        *ptrToPtrOfSharedTokens = NULL;
    }
}

void receiveToken(const int myPID, int &inCS, int &tokenHere, std::map<int, RequestArrayNode> &reqArray,
                  Token **ptrToPtrOfSharedTokens, const int numNodes, const Time &start, FILE *fp)
{

    tokenHere = TRUE;

    if ((*ptrToPtrOfSharedTokens)->elecID == myPID)
        inCS = TRUE;
    else
    {
        int nbrVal = getNeighborIDfromReqArray(myPID, (*ptrToPtrOfSharedTokens)->elecID, reqArray);
        (*ptrToPtrOfSharedTokens)->type = messageType::TOKEN;
        (*ptrToPtrOfSharedTokens)->senderID = myPID;
        tokenHere = FALSE;

        printf("Node %d: receiveToken -> Sending Token for %d to neighbor %d\n", myPID, (*ptrToPtrOfSharedTokens)->elecID, nbrVal);
        long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
        fprintf(fp, "%d sends TOKEN to neighbor %d for %d with lud |%s| at %lld\n", myPID, nbrVal, (*ptrToPtrOfSharedTokens)->elecID, getLudFromToken(numNodes, ptrToPtrOfSharedTokens).c_str(), sysTime);
        fflush(fp);
        if (sendMessage(myPID, nbrVal, **ptrToPtrOfSharedTokens) == false)
        {
            printf("ERROR :: Node %d: receiveToken -> Unable to send TOKEN for %d to neighbor %d\n", myPID, (*ptrToPtrOfSharedTokens)->elecID, nbrVal);
            exit(EXIT_FAILURE);
        }
        *ptrToPtrOfSharedTokens = NULL;
    }
}

void receiveRequest(const int myPID, int &inCS, int &tokenHere, LLONG &logicalClock, std::map<int, RequestArrayNode> &reqArray, RequestMessage *request,
                    std::vector<int> &neighbors, Token **ptrToPtrOfSharedTokens, const int numNodes, const Time &start, FILE *fp)
{
    bool isRequestAlreadyPresent = false;

    for (auto &nbr : reqArray)
    {
        std::list<RequestID>::iterator it = nbr.second.begin();
        while (it != nbr.second.end())
        {
            if (it->reqOriginId == request->reqOriginId)
            {
                if (it->reqTime < request->reqTime)
                {
                    nbr.second.erase(it++);
                    continue;
                }
                else
                {
                    isRequestAlreadyPresent = true;
                }
            }
            it++;
        }
    }

    if (!isRequestAlreadyPresent)
    {
        logicalClock = std::max(logicalClock, request->reqTime) + 1;
        reqArray[request->senderID].push_back(RequestID{
            request->reqOriginId,
            request->reqTime});

        std::vector<int> alreadySeen = extractProcessIDFromAlreadySeen(request->alreadySeen);

        request->senderID = myPID;
        strcpy(request->alreadySeen, unionAlreadySeenNeighbors(alreadySeen, neighbors).c_str());

        for (const int neighbor : neighbors)
        {
            if (find(alreadySeen.begin(), alreadySeen.end(), neighbor) == alreadySeen.end())
            {
                long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
                fprintf(fp, "%d sends REQUEST to %d for %d at %lld\n", myPID, neighbor, request->reqOriginId, sysTime);
                fflush(fp);
                if (sendMessage(myPID, neighbor, *request) == false)
                {
                    printf("ERROR :: Node %d: receiveRequest -> Unable to send REQUEST to %d for %d\n", myPID, neighbor, request->reqOriginId);
                    exit(EXIT_FAILURE);
                }
            }
        }
        if (tokenHere == TRUE && inCS == FALSE)
            transmitToken(myPID, tokenHere, logicalClock, reqArray, ptrToPtrOfSharedTokens, numNodes, start, fp);
    }
}

void requestCS(const int myPID, int &inCS, int &tokenHere, LLONG &logicalClock,
               std::vector<int> &neighbors, const Time &start, FILE *fp, std::mutex *lock)
{
    lock->lock();

    if (tokenHere == TRUE)
    {
        inCS = TRUE;
        lock->unlock();
    }
    else
    {
        RequestMessage message;
        message.reqTime = logicalClock;
        lock->unlock();

        message.type = messageType::REQUEST;
        message.senderID = myPID;
        message.reqOriginId = myPID;

        strcpy(message.alreadySeen, constructAlreadySeenString(myPID, neighbors).c_str());

        for (const int nbr : neighbors)
        {
            long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
            fprintf(fp, "%d sends REQUEST to %d at %lld\n", myPID, nbr, sysTime);
            fflush(fp);
            if (sendMessage(myPID, nbr, message) == false)
            {
                std::cout << "ERROR :: Node " << myPID << ": requestCS -> Unable to send REQUEST to " << nbr << std::endl;
                exit(EXIT_FAILURE);
            }
        }
    }

    while (true)
    {
        lock->lock();
        if (inCS == TRUE)
        {
            lock->unlock();
            break;
        }
        lock->unlock();
    }
}

void exitCS(const int myPID, int &inCS, int &tokenHere, LLONG &logicalClock, std::map<int, RequestArrayNode> &reqArray,
            Token **ptrToPtrOfSharedTokens, const int numNodes, const Time &start, FILE *fp, std::mutex *lock)
{
    lock->lock();
    inCS = false;
    transmitToken(myPID, tokenHere, logicalClock, reqArray, ptrToPtrOfSharedTokens, numNodes, start, fp);
    lock->unlock();
}
