# p2pfileshare
Simple 4 node P2P file sharing system written in 10 hours for a hackathon

Used C99 and Pthreads
To compile: make
To clean: make clean
To run (on each node): make run
Important notes:
Due to the shared file system nature of the machines used to develop, the program is designed to treat folders “files[1-4]” as the directories for P2P. The first will be associated with the first ip in the hostfile and so on.
