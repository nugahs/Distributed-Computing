#include <iostream>
#include <iomanip>
#include <vector>
#include <mpi.h>
#include <cstdlib>
#include <chrono>
#include <ctime>
using namespace std;
void end_all_processes(int size);

int main(int argc, char *argv[]) 
{
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 2) {
        if (rank == 0)
            std::cerr << "Usage: " << argv[0] << " <m>" << std::endl;
        MPI_Finalize();
        return 1;
    }

    int m = std::atoi(argv[1]); // Number of messages to send

    srand(time(NULL) + rank); // Seed the random number generator

    std::vector<std::vector<int>> vector_clock(size, std::vector<int>(size, 0)); // Vector clock for each process

    int messages_sent = 0; // Number of messages sent by this process
    
    MPI_Status status;

    while (messages_sent < m) {
        int action = rand() % 3; // Randomly choose action: 0 - execute, 1 - send, 2 - receive
        auto currentTime = std::chrono::system_clock::now();
        std::time_t currentTime_t = std::chrono::system_clock::to_time_t(currentTime);

        // Convert the time point to a string representation
        std::string currentTimeStr = std::ctime(&currentTime_t);

        if (action == 0) {
            // Execute event
            // Increment own vector clock
            ++vector_clock[rank][rank];
            std::cout << "Process " << rank << " executing Internal event e"<< rank << messages_sent << "at " << currentTimeStr <<"vc: [";
            for (int i = 0; i < size; ++i) {
                std::cout << vector_clock[rank][i] << " ";
            }++messages_sent;
            std::cout << "]"<<std::endl;
        } else if (action == 1) {
            // Send message
            int sProcess = (rank + 1) % size; // Randomly choose a process to send message to
            // Increment own vector clock
            ++vector_clock[rank][rank];
            std::cout << "Process" << rank << " sending message m"<<rank<<messages_sent <<" to process " << sProcess << " at "<< currentTimeStr << "vc:[";
            for (int i = 0; i < size; ++i) {
                std::cout << vector_clock[rank][i] << " ";
            }
            std::cout <<"]" <<std::endl;
            // Send message with vector clock
            MPI_Send(vector_clock[rank].data(), size, MPI_INT, sProcess, 0, MPI_COMM_WORLD);
            ++messages_sent; // Increment messages sent counter
        } else {
            // Receive message
            int rProcess = (rank + 1) % size; // Randomly choose a process to receive message from
            std::vector<int> received_clock(size);
            MPI_Recv(received_clock.data(), size, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            // Update own vector clock
            for(int i=0;i<size;i++)
            cout<< received_clock[i];
           // for (int i = 0; i < size; ++i) {
             //   vector_clock[rank][i] = std::max(vector_clock[rank][i], received_clock[i]);
            //}
            // Increment own vector clock
            ++vector_clock[rank][rank];
            //std::cout << "Process" << rank << " received message m"<<status.MPI_SOURCE<<vector_clock[rank][status.MPI_SOURCE]<<"from process " << status.MPI_SOURCE << " at " << currentTimeStr <<" vc:[";
            for (int i = 0; i < size; ++i) {
                std::cout << vector_clock[rank][i] << " ";
            }
            std::cout << "]"<<std::endl;
        }
    }

    std::cout << "Process " << rank << " has sent " << m << " messages. Terminating." << std::endl;

    end_all_processes(size);

    MPI_Finalize();
    return 0;
}

void end_all_processes(int size) {
    int signal[3];
    signal[0] = 0; // Terminate signal
    for (int i = 1; i < size; ++i) {
        MPI_Send(signal, 3, MPI_INT, i, 0, MPI_COMM_WORLD);
    }
    MPI_Finalize();
}
