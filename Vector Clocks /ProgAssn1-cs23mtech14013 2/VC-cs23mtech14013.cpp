#include <iostream>
#include <ctime>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>
#include <algorithm>
#include <random>
#include <cstring>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>

using namespace std;

#define PORT 10000

tm *ltm;
time_t now;
int n, lambda, m;
double alpha;
vector<vector<int>> graph;
FILE *fd;
mutex mtx;
int msgs_rcv = 0;



void updateVectorClock(vector<int> &vc, int event, int pid, const vector<int> &vc_recv = vector<int>())
{

    vc[pid]++;
    for (int i = 0; i < ( min(vc.size(), vc_recv.size())); ++i) {
        vc[i] = max(vc[i], vc_recv[i]);
    }


}

void displayVectorClock(vector<int> &vc)
{
    fprintf(fd, "[ ");
    for (auto &i : vc)
        fprintf(fd, "%d ", i);
    fprintf(fd, "]\n");
}
double genExp(float lambda)
{
    default_random_engine generate;
    exponential_distribution<double> distribution(lambda);
    return distribution(generate);
}
void sendMsgs(int sock, int pid, vector<int> &msg)
{
    struct sockaddr_in cliaddr;
    int sock_cli;
    if ((sock_cli = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Client socket creation failed for process %d\nPlease try again in some time\n", pid);
        exit(1);
    }
    memset(&cliaddr, 0, sizeof(cliaddr));

    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    cliaddr.sin_port = htons(PORT + pid);

    if (connect(sock_cli, (sockaddr *)&cliaddr, sizeof(sockaddr)) < 0)
    {
        cout << "Connection failed with process " << pid << "errno = " << errno << "\nPlease try again in some time\n"
             << flush;
        exit(1);
    }

    int send_msg[n + 2];
    copy(msg.begin(), msg.end(), send_msg);

    while (send(sock_cli, send_msg, sizeof(send_msg), 0) < 0); 
    close(sock_cli);
}

void simulateEvents(int sock, int pid, vector<int> &vc)
{
    int internal = 0, message = 0;
    double sleep;
    double totalEvents = m * (alpha + 1);
    int turn = -1;
    int choice;

    for (int i = 0; i < ceil(totalEvents); ++i)
    {
        choice = rand() % 3;

        if (((choice == 0 || choice == 1) && internal < m * alpha) || message == m)
        {
            updateVectorClock(vc, 0, pid);
            internal++;

            mtx.lock();
            now = time(0);
            ltm = localtime(&now);
            fprintf(fd, "Process%d executes internal event e%d%d at %d:%d, vc: ", pid, pid, internal, ltm->tm_hour, ltm->tm_min);
            displayVectorClock(vc);
            mtx.unlock();

            sleep = genExp(lambda);
            usleep(sleep * 1000);
        }
        else
        {
            turn = (turn + 1) % graph[pid].size();
            int pid2 = graph[pid][turn];

            updateVectorClock(vc, 1, pid);
            message++;

            mtx.lock();
            now = time(0);
            ltm = localtime(&now);
            fprintf(fd, "Process%d sends message m%d%d to process%d at %d:%d, vc: ", pid, pid, message, pid2, ltm->tm_hour, ltm->tm_min);
            displayVectorClock(vc);
            vector<int> send_msg(vc.begin(), vc.end());
            send_msg.push_back(message);
            send_msg.push_back(pid);
            mtx.unlock();

            sendMsgs(sock, pid2, send_msg);
            sleep = genExp(lambda);
            usleep(sleep * 1000);
        }
    }
}

void recvMsg(int sock, int pid, vector<int> &vc)
{
    struct timeval tv;
    tv.tv_sec = 2; /* timeout in Seconds */
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    struct sockaddr_in cliaddr;
    socklen_t from_len;
    from_len = sizeof(struct sockaddr_in);
    memset(&cliaddr, 0, sizeof(cliaddr));
    int setbit = 0;
    long arg;
    arg = fcntl(sock, F_GETFL, NULL);
    arg |= O_NONBLOCK;
    fcntl(sock, F_SETFL, arg);

    while (msgs_rcv < m * n)
    {
        int recv_int[n + 2];
        int sock_cli;

        if (msgs_rcv == m * n)
            break;

        while ((sock_cli = accept(sock, (sockaddr *)&cliaddr, &from_len)) < 0)
        {
            if (msgs_rcv == m * n)
            {
                setbit = 1;
                break;
            }
        }
        if (setbit)
            break;
        int success = recv(sock_cli, recv_int, sizeof(recv_int), 0);

        if (success > 0)
        {

            int message_number = recv_int[n];
            int pid2 = recv_int[n + 1];

            vector<int> vc_recv(n);

            for (int i = 0; i < n; ++i)
                vc_recv[i] = recv_int[i];

            updateVectorClock(vc, 2, pid, vc_recv);

            mtx.lock();
            now = time(0);
            ltm = localtime(&now);
            msgs_rcv = msgs_rcv + 1;
            fprintf(fd, "Process%d receives m%d%d from process%d at %d:%d, vc: ", pid, pid2, message_number, pid2, ltm->tm_hour, ltm->tm_min);
            displayVectorClock(vc);
            mtx.unlock();
        }
        else if (success == 0)
            cout << "Empty message received!!\n"
                 << flush;

        close(sock_cli);
    }
}

void startProcess(int id)
{
    struct sockaddr_in servaddr;
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation failed for process id %d, Error no = %d\nPlease try again in some time\n", id, errno);
        exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT + id);

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("Socket binding failed for Process %d, errno = %d\nPlease try again in some time\n", id, errno);
        exit(1);
    }

    if (listen(sock, n - 1) < 0)
    {
        printf("Listening failed for process %d, errno = %d\nPlease try again in some time\n", id, errno);
        exit(1);
    }

    thread event_thread, receive_thread;
    vector<int> vc(n, 0);

    event_thread = thread(simulateEvents, sock, id, ref(vc));
    receive_thread = thread(recvMsg, sock, id, ref(vc));

    receive_thread.join();
    event_thread.join();

    close(sock);
}

int main()
{

    ifstream in("inp-params.txt");
    fd = fopen("VC-log_TCP.txt", "w+");

    if (in.is_open())
    {
        in >> n >> lambda >> alpha >> m;
        graph.resize(n);

        for (int i = 0; i < n; ++i)
        {
            int vertex, edge;
            in >> vertex;

            string line;
            getline(in, line);
            istringstream iss(line);
            while (iss >> edge && edge > 0)
            {
                graph[vertex - 1].push_back(edge - 1);
            }
        }

        cout << "n: " << n << ", lambda: " << lambda << ", alpha: " << alpha << ", m: " << m << endl;
        cout << "Graph Topology:" << endl;
        for (int i = 0; i < n; ++i)
        {
            cout << i + 1 << ": ";
            for (int j : graph[i])
            {
                cout << j + 1 << " ";
            }
            cout << endl;
        }

        thread th[n];

        for (int id = 0; id < n; id++)
        {
            th[id] = thread(startProcess, id);
        }

        for (int id = 0; id < n; id++)
        {
            th[id].join();
        }
    }

    fclose(fd);
    cout << "Average message size sent = " << float(n + 2) << endl;
    return 0;
}
