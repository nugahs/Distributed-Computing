Compililation Command for VC-CS23MTECH14013.cpp and SK-CS23MTECH14013.cpp

g++ VC-cs23mtech14013.cpp -std=c++14 -o vc -pthread

g++ SK-cs23mtech14013.cpp -std=c++14 -o sk -pthread

Run as:

./vc
./sk

It produces following Files : 

                 VC-log.txt
                 SK-log.txt

Input file format : 
name of file : inp-params.txt
n lambda alpha m
vertexi no.of edges e1 e2 e3 ....en

ex:
for number of processes = 10, and a complete graph:

10 5 1.5 50
1 2 3 4 5 6 7 8 9 10
2 1 3 4 5 6 7 8 9 10
3 1 2 4 5 6 7 8 9 10 
4 1 2 3 5 6 7 8 9 10 
5 1 2 3 4 6 7 8 9 10 
6 1 2 3 4 5 7 8 9 10 
7 1 2 3 4 5 6 8 9 10 
8 1 2 3 4 5 6 7 9 10 
9 1 2 3 4 5 6 7 8 10
10 1 2 3 4 5 6 7 8 9 




For code done using MPI  (VC-MPI):

Compilation command for Linux Kali:
mpic++ -o demo filename.cpp 
If this gives error " Slots not available " the use the below command 
mpiexec --oversubscribe -n 5 ./mpicode 12
n is num of processes and 12 is value of m 


Exceution command for Linux Kali:
mpiexec ./demo 

Input file not required for this code

