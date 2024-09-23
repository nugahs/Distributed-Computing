#include <iostream>
#include <fcntl.h>
#include <utility>
#include <thread>
#include<mutex>
#include<math.h>
#include<sstream>
#include<iostream>
#include<random>
#include<arpa/inet.h>
#include<unistd.h>
#include<atomic>
#include<stdlib.h>
#include<string>
#include<thread>
#include <cstring>
#include<time.h>
#include<vector>
#include<semaphore.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<error.h>
#include<netinet/in.h>
#include<fstream>


using namespace std;

#define PORT 10000
tm *ltm;
time_t now;
int num_processes, lambda, num_messages;
double alpha;
vector< vector<int> > communication_graph; 
FILE *log_file;
mutex mtx;
int messages_received = 0;
int total_message_size = 0;



void displayVectorClock(vector<int> &vc)
{
	fprintf(log_file, "[ ");
	for(auto &i:vc)
		fprintf(log_file, "%d ",i);
	fprintf(log_file, "]\n");
}
void vectorClockUpdate(vector<int> &vc, int event, int pid, int pid2, vector<int> &ls, vector<int> &lu, int vec_tuple_recv[100][2]={0})
{

	lu[pid] = ++vc[pid];


	for (int i = 1; i <= num_processes && event == 2 && vec_tuple_recv[i][0] > -1; ++i) {
        int index = vec_tuple_recv[i][0];
        int value = vec_tuple_recv[i][1];
        if (vc[index] < value) {
            vc[index] = value;
            lu[index] = vc[pid];
        }
    }


}

double expRand(float lambda)
{
	default_random_engine generator;
	exponential_distribution<double> distribution(lambda);
	return distribution(generator);
}

void receiveEvent(int sock, int pid, vector<int> &vc, vector<int> &ls, vector<int> &lu)
{
	struct timeval timeout;
	timeout.tv_sec = 2;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(char *)&timeout,sizeof(struct timeval));	
	
	struct sockaddr_in client_address;
	socklen_t address_length;
	address_length = sizeof(struct sockaddr_in);
	memset(&client_address, 0, sizeof(client_address));
	int setbit=0;

	long arg;
	  arg = fcntl(sock, F_GETFL, NULL); 
	  arg |= O_NONBLOCK; 
	  fcntl(sock, F_SETFL, arg); 

	while(messages_received < num_messages*num_processes)
	{
		int received_int[num_processes+1][2];
		int client_socket;

		for(int i = 0;i<num_processes+1;++i)
		{
			received_int[i][0] = -1;
			received_int[i][1] = -1;
		}
		if(messages_received == num_messages*num_processes)
			break;

		while((client_socket = accept(sock, (sockaddr *)&client_address, &address_length))<0)
		{
			if(messages_received == num_messages*num_processes)
			{
				setbit=1;
				break;
			}
		}
		if(setbit)
			break;
		int success = recv(client_socket, received_int, sizeof(received_int), 0);

		if(success > 0)
		{
			int message_number = received_int[0][0];
			int pid2 = received_int[0][1];
			vectorClockUpdate(vc, 2, pid, pid2, ls, lu, received_int);

			mtx.lock();
			now = time(0);
			ltm = localtime(&now);
			messages_received++;
			fprintf(log_file, "Process%d receives m%d%d from process%d at %d:%d, vc: ", pid, pid2, message_number, pid2, ltm->tm_hour, ltm->tm_min  );
			displayVectorClock(vc);

			mtx.unlock();
		}	
		else if(success == 0) 
			cout<<"Empty message received!!\n"<<flush;

		close(client_socket);
	}
}

void sendToClient(int sock, int pid, vector<int> &vc, vector<int> &ls, vector<int> &lu, int message, int pid2)
{
	struct sockaddr_in client_address;
	int client_socket;
	if((client_socket = socket(AF_INET, SOCK_STREAM, 0))<0)
	{
		printf("client socket failed for process %d\nPlease try again in some time\n",pid);
		exit(1);
	}
	memset(&client_address, 0, sizeof(client_address));
	client_address.sin_family = AF_INET;
	client_address.sin_addr.s_addr = inet_addr("127.0.0.1");
	client_address.sin_port = htons(PORT + pid);	


	if(connect(client_socket, (sockaddr *)&client_address, sizeof(sockaddr))<0)
	{
		cout<<"connection failed with process " <<pid<<", errno = " << errno<<"\nPlease try again in some time\n"<<flush;
		exit(1);
	}

	vector<int> update;

	for(int k = 0;k<lu.size();++k)
	{
		if(lu[k]>ls[pid])
			update.push_back(k);
	}

	mtx.lock();
	total_message_size += (update.size()+1)*2;
	mtx.unlock();

	int send_msg[update.size()+1][2];
	send_msg[0][0] = message;
	send_msg[0][1] = pid2;
	for(int i = 1;i<update.size()+1;++i)
	{	
		send_msg[i][0] = update[i-1];
		send_msg[i][1] = vc[update[i-1]];
		
	}

	while(send(client_socket, send_msg, sizeof(send_msg), 0)<0);

	close(client_socket);
}

void internalAndSendEvent(int sock, int pid, vector<int> &vc, vector<int> &ls, vector<int> &lu)
{
	int internal = 0, message = 0;
	double sleep;
	double total_events = num_messages*(alpha+1);
	int turn = -1;
	int choice;

	for(int i = 0;i < ceil(total_events); ++i)
	{
		choice = rand()%3;

		if( ((choice == 0 || choice == 1) && internal<num_messages*alpha) || message == num_messages)
		{

			lu[pid] = ++vc[pid];
			internal++;

			mtx.lock();
			now = time(0);
			ltm = localtime(&now);
			fprintf(log_file, "Process%d executes internal event e%d%d at %d:%d, vc: ",pid,pid,internal, ltm->tm_hour, ltm->tm_min );
			displayVectorClock(vc);
			mtx.unlock();

			sleep = expRand(lambda);
			usleep(sleep*1000);
		}
		else
		{
			turn = (turn + 1)%communication_graph[pid].size();
			int pid2 = communication_graph[pid][turn];

			lu[pid] = ++vc[pid];
			message++;

			mtx.lock();
			now = time(0);
			ltm = localtime(&now);
			fprintf(log_file, "Process%d sends message m%d%d to process%d at %d:%d, vc: ",pid,pid,message,pid2,ltm->tm_hour,ltm->tm_min  );
			displayVectorClock(vc);

			mtx.unlock();

			sendToClient(sock, pid2, vc, ls, lu, message, pid);
			ls[pid2] = vc[pid];
			sleep = expRand(lambda);
			usleep(sleep*1000);
		}	
	}
}


void individualProcess(int id)
{
	struct sockaddr_in server_address;
	int socket_descriptor;
	if((socket_descriptor = socket(AF_INET, SOCK_STREAM, 0))<0)
	{
		printf("Socket creation failed for process id %d, errno = %d\nPlease try again in some time\n", id, errno);
		exit(1);
	}

    bzero(&server_address, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(PORT+id);

    if (bind(socket_descriptor, (struct sockaddr *)&server_address, sizeof(server_address)) != 0 )
    {
        printf("Socket binding failed for Process %d, errno = %d\nPlease try again in some time\n", id, errno);
    	exit(1);
    }
	
	if(listen(socket_descriptor, num_processes-1)<0)
	{
		printf("listening failed for process %d, errno = %d\nPlease try again in some time\n", id, errno);
		exit(1);
	}

	thread event_thread, receive_thread;
	vector<int> vector_clock(num_processes, 0);
	vector<int> last_sent(num_processes, 0); 
	vector<int> last_update(num_processes, 0); 

	event_thread = thread(internalAndSendEvent, socket_descriptor, id, ref(vector_clock), ref(last_sent), ref(last_update)); 
	receive_thread = thread(receiveEvent, socket_descriptor, id, ref(vector_clock), ref(last_sent), ref(last_update));

	receive_thread.join();
	event_thread.join();

	close(socket_descriptor);
}

int main()
{
	ifstream input_file("inp-params.txt");

	log_file = fopen ("SK-log_TCP.txt","w+");

	
		input_file>>num_processes>>lambda>>alpha>>num_messages;
		communication_graph.resize(num_processes);
		for (int i = 0; i < num_processes; ++i) 
    	{
        	int vertex, edge;
        	input_file >> vertex;
        	string line;
        	getline(input_file, line);
        	istringstream iss(line);
        	while (iss >> edge && edge > 0) 
        	{
            	communication_graph[vertex - 1].push_back(edge - 1);
        	}
    	}

    	cout << "Number of processes: " << num_processes << ", Lambda: " << lambda << ", Alpha: " << alpha << ", Number of messages: " << num_messages << endl;
    	cout << "Communication Graph:" << endl;
    	for (int i = 0; i < num_processes; ++i) 
    	{
        	cout << i + 1 << ": ";
        	for (int j : communication_graph[i]) {
            	cout << j + 1 << " ";
        	}
        	cout << endl;
    	}

		thread process_threads[num_processes];

		for(int id=0; id<num_processes; ++id)
		{
			process_threads[id] = thread(individualProcess, id);
		}
	
		for(int id=0; id<num_processes; ++id)
		{
			process_threads[id].join();
		}

	fclose(log_file);
	cout<<"Average message size sent = "<<float(total_message_size)/(num_processes*num_messages*1.0)<<endl;
	return 0;
}
