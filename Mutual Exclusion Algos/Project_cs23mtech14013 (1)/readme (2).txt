1. To run Pathreversal Program : 


To compile pathreversal.cpp, use the following compile command
g++ -std=c++14 -pthread pathreversal.cpp -o sock


To run ./sock, use the following command
./sock [input file path] [starting port number]


Ex: If pathreversal.cpp and inp-params.txt is in the same directory
./sock inp-params.txt 9000


2. To run Helary’s Mutual Exclusion Algorithm
To Compile
g++ -std=c++14 -pthread main.cpp -o abc
To run use below command 
./abc inp-params.txt 9009
9009 denotes the starting value of the range of port numbers where the port numbers for the different nodes will be allocated.
If we run code again and again then it is recommended to change port number to avoid getting port busy issues.