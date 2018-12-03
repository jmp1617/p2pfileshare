#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <dirent.h>    
#include <errno.h>        
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define PORT 12347
#define SPORT 12348

struct file_data_s{
	char* file_name;
	char* ip;
};

typedef struct file_data_s* FileData;

char ** parse_hostfile(char* filename){
	FILE *fp;
	fp = fopen(filename, "r");
	if (fp == NULL){
        	printf("Could not open file %s",filename);
        	return NULL;
    	}
	char** hosts = (char**)calloc(4,sizeof(char*));
	for(int ip = 0; ip<4; ip++){
		hosts[ip] = (char*)calloc(20,sizeof(char));
		fgets(hosts[ip],20,fp);
		strtok(hosts[ip],"\n");
	}
	return hosts;
}

void * client(void* arg){
	char*** ip_array_p = arg;
	char** ip_array = *ip_array_p;
	printf("Client\n");
	//if ip is first then you are the leader
	int fd;
 	struct ifreq ifr;
 	fd = socket(AF_INET, SOCK_DGRAM, 0);
 	ifr.ifr_addr.sa_family = AF_INET;
 	strncpy(ifr.ifr_name, "eno1", IFNAMSIZ-1); 
 	ioctl(fd, SIOCGIFADDR, &ifr);
 	close(fd);
 	char* my_ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
	printf("My ip is: %s\n",my_ip);

	int leader = 0;
	if(strcmp(my_ip,ip_array[0])==0)
		leader = 1;

	printf("Leader: %d\n",leader);

	int files_read = 0;
	FileData file_data[20];
	if(leader){//leader server
		for(int file = 0; file<20; file++){
			file_data[file] = calloc(1,sizeof(struct file_data_s));
			file_data[file]->ip = calloc(20,sizeof(char));
			file_data[file]->file_name = calloc(20,sizeof(char));  
		}
		//read leaders files
		DIR *dir;
		struct dirent *ent;
		if((dir = opendir("./files1/"))!=NULL){
			while(((ent = readdir(dir))!=NULL)&&files_read<5){
				char* filename = ent->d_name;
				if(strcmp(filename, ".")!=0&&strcmp(filename,"..")!=0){
					strncpy(file_data[files_read]->ip,my_ip,20);
					strncpy(file_data[files_read]->file_name,filename,20);			
					files_read++;
				}
			}
			closedir(dir);
		}
		else{
			printf("Could not open files folder. Make sure the files directory exists\n");
			return NULL;
		}
		//accept files from others
		int opt = 1;   
    		int master_socket , addrlen , new_socket , client_socket[3] ,  
          		max_clients = 3 , activity, i , valread , sd;   
    		int max_sd;   
    		struct sockaddr_in address;   
         
    		char buffer[1025];  //data buffer of 1K  
         
    		//set of socket descriptors  
    		fd_set readfds;

		//initialise all client_socket[] to 0 so not checked  
    		for (i = 0; i < max_clients; i++)   
    		{   
        		client_socket[i] = 0;   
    		}  
		//create a master socket  
    		if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)   
    		{   
        		perror("socket failed");   
        		exit(EXIT_FAILURE);   
    		}
    		if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,  
          		sizeof(opt)) < 0 )   
    		{   
        		perror("setsockopt");   
        		exit(EXIT_FAILURE);   
    		}
		//type of socket created  
    		address.sin_family = AF_INET;   
    		address.sin_addr.s_addr = INADDR_ANY;   
    		address.sin_port = htons( PORT );

    		if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)   
    		{   
        		perror("bind failed");   
        		exit(EXIT_FAILURE);   
    		}   
    		printf("Listener on port %d \n", PORT);

		//try to specify maximum of 3 pending connections for the master socket  
    		if (listen(master_socket, 3) < 0)   
    		{   
        		perror("listen");   
        		exit(EXIT_FAILURE);   
    		}

		//accept the incoming connection  
    		addrlen = sizeof(address);   
   		puts("Waiting for file lists ...");
		int successful_connect = 0;
		while(1){
			//clear the socket set  
        		FD_ZERO(&readfds);   
     
        		//add master socket to set  
        		FD_SET(master_socket, &readfds);   
        		max_sd = master_socket; 
			//add child sockets to set  
        		for ( i = 0 ; i < max_clients ; i++)   
        		{   
            			//socket descriptor  
            			sd = client_socket[i];   
                 
            			//if valid socket descriptor then add to read list  
            			if(sd > 0)   
                			FD_SET( sd , &readfds);   
                 
            			//highest file descriptor number, need it for the select function  
            			if(sd > max_sd)   
                			max_sd = sd;   
        		}
			//wait for an activity on one of the sockets , timeout is NULL ,  
        		//so wait indefinitely  
        		activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);   
       
        		if ((activity < 0) && (errno!=EINTR))   
        		{   
            			printf("select error");   
        		}
			
        		if (FD_ISSET(master_socket, &readfds))   
        		{   
            			if ((new_socket = accept(master_socket,  
                    			(struct sockaddr *)&address, (socklen_t*)&addrlen))<0)   
            			{   
                			perror("accept");   
                			exit(EXIT_FAILURE);   
            			}   
              
                 
				//add new socket to array of sockets  
            			for (i = 0; i < max_clients; i++)   
            			{   
                			//if position is empty  
                			if( client_socket[i] == 0 )   
                			{   
                    				client_socket[i] = new_socket;    
                    				break;   
                			}   
            			}
    
			}
        		for (i = 0; i < max_clients; i++)   
        		{   
            			sd = client_socket[i];   
                 
            			if (FD_ISSET( sd , &readfds))   
            			{   
                    			//Check if it was for closing , and also read the  
                			//incoming message  
                			if ((valread = read( sd , buffer, 1024)) == 0)   
                			{	   
                    				close( sd );   
                    				client_socket[i] = 0;   
                			}   
                     
                			else 
                			{   
                    				//set the string terminating NULL byte on the end  
                    				//of the data read  
                    				buffer[valread] = '\0'; 
						printf("%s\n",buffer);
						//process files
						char* pt;
						pt = strtok(buffer,",");
						char ip[20] = {0};
						strncpy(ip,pt,20);
						pt = strtok(NULL,",");
						for(int i =0;i<5;i++){
							 
							strncpy(file_data[files_read]->ip,ip,20);
							strncpy(file_data[files_read]->file_name,pt,strlen(pt));			
							files_read++;
							pt = strtok(NULL,",");
						}		
						//
						successful_connect++;
						if(successful_connect==3)
							break;  
                			}			   
            			}   
 		        }
			if(successful_connect==3)
				break; 
		}
		printf("File lists successfully collected: connections closed\nMaster list:\n");
		for(int i = 0; i < 20; i++){
			printf("%s -> %s\n",file_data[i]->ip,file_data[i]->file_name);
		}
		//open up server to give out file names
		printf("Waiting for file name queries...\n");
		while(1){
			//clear the socket set  
        		FD_ZERO(&readfds);   
     
        		//add master socket to set  
        		FD_SET(master_socket, &readfds);   
        		max_sd = master_socket; 
			//add child sockets to set  
        		for ( i = 0 ; i < max_clients ; i++)   
        		{   
            			//socket descriptor  
            			sd = client_socket[i];   
                 
            			//if valid socket descriptor then add to read list  
            			if(sd > 0)   
                			FD_SET( sd , &readfds);   
                 
            			//highest file descriptor number, need it for the select function  
            			if(sd > max_sd)   
                			max_sd = sd;   
        		}
			//wait for an activity on one of the sockets , timeout is NULL ,  
        		//so wait indefinitely  
        		activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);   
       
        		if ((activity < 0) && (errno!=EINTR))   
        		{   
            			printf("select error");   
        		}
			
        		if (FD_ISSET(master_socket, &readfds))   
        		{   
            			if ((new_socket = accept(master_socket,  
                    			(struct sockaddr *)&address, (socklen_t*)&addrlen))<0)   
            			{   
                			perror("accept");   
                			exit(EXIT_FAILURE);   
            			}   
              
                 
				//add new socket to array of sockets  
            			for (i = 0; i < max_clients; i++)   
            			{   
                			//if position is empty  
                			if( client_socket[i] == 0 )   
                			{   
                    				client_socket[i] = new_socket;    
                    				break;   
                			}   
            			}
    
			}
        		for (i = 0; i < max_clients; i++)   
        		{   
            			sd = client_socket[i];   
                 
            			if (FD_ISSET( sd , &readfds))   
            			{   
                    			//Check if it was for closing , and also read the  
                			//incoming message  
                			if ((valread = read( sd , buffer, 1024)) == 0)   
                			{	   
                    				close( sd );   
                    				client_socket[i] = 0;   
                			}   
                			else 
                			{   
                    				//set the string terminating NULL byte on the end  
                    				//of the data read  
                    				buffer[valread] = '\0'; 
						printf("Recieved request for: %s\n",buffer);
						//find the file ip
						int found = 0;
						char file_ip[20];
						char* not_f = "File Not Found";
						for(int i = 0; i<files_read; i++){
							if(strcmp(file_data[i]->file_name,buffer)==0){
								strncpy(file_ip,file_data[i]->ip,20);
								found = 1;
							}
						}
						if(found){
							printf("Found at %s, sending...\n",file_ip);
							send(sd, file_ip, strlen(file_ip), 0);
						}
						else{
							printf("Letting them know it doest exist...\n");
							send(sd, not_f, strlen(not_f), 0);
						}
                			}			   
            			}   
 		        }
		}
	}
	else{//not leader
		char* filedirname="";
		if(strcmp(my_ip,ip_array[1])==0)
			filedirname = "files2";
		else if(strcmp(my_ip,ip_array[2])==0)
			filedirname = "files3";
		else if(strcmp(my_ip,ip_array[3])==0)
			filedirname = "files4";
		//get file list
		char buffer[200] = {0};
		strncpy(buffer,my_ip,strlen(my_ip));
		int buffer_pointer = strlen(my_ip);	
		strncpy((buffer+buffer_pointer),",",1);
		buffer_pointer++;
		DIR *dir;
		struct dirent *ent;
		char file_path[30];
		strcat(file_path,"./");
		strcat(file_path,filedirname);
		strcat(file_path,"/");
		if((dir = opendir(file_path))!=NULL){
			int files_read = 0;
			while(((ent = readdir(dir))!=NULL)&&files_read<5){
				char* filename = ent->d_name;
				if(strcmp(filename, ".")!=0&&strcmp(filename,"..")!=0){
					strncpy((buffer+buffer_pointer),filename,strlen(filename));
					buffer_pointer+=strlen(filename);
					strncpy((buffer+buffer_pointer),",",1);
					buffer_pointer++;
					files_read++;
				}
			}
			closedir(dir);
		}
		else{
			printf("Could not open files folder. Make sure the \"files\" directory exists\n");
			return NULL;
		}
		
		//send the buffer
		char* SERVER_IP = ip_array[0];
		
		struct sockaddr_in address;
        	int sock = 0, valread;
        	struct sockaddr_in serv_addr;
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        	{
                	printf("\n Socket creation error \n");
                	return NULL;
        	}
		memset(&serv_addr, '0', sizeof(serv_addr));

        	serv_addr.sin_family = AF_INET;         // Address family
        	serv_addr.sin_port = htons(PORT);       // Server port in network byte order
		if(inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr)<=0)
        	{
                	printf("\nInvalid address/ Address not supported \n");
                	return NULL;
        	}
		printf("Finding Leader...\n");
		while (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){}
		printf("Leader found, sending file list to leader\n");
		send(sock, buffer, strlen(buffer), 0);
		printf("File list sent\n");
		//get input
		char location[20] = {0};
		char requested_file[20] = {0};
		int found_file = 0;
		while(1){
			char input[20] = {0};
			printf("Enter a file name to query (max 20) -> ");
			scanf("%s",input);
			printf("Asking where %s is...\n",input);
			send(sock, input, strlen(input), 0);
			printf("Waiting for an answer...\n");
			char response[1024]={0};
			read(sock, response, 1024);
			if(strcmp(response,"File Not Found")){
				printf("File \"%s\" found at: %s\n",input,response);
				send(sock, NULL, 0, 0);//close
				found_file = 1;
				strncpy(location,response,20);
				strncpy(requested_file,input,20);
			}
			else{
				printf("File was not found\n");
				send(sock,NULL,0,0);
			}

		if(found_file){
			if(strcmp(location,my_ip)){
				printf("Asking file server for the file...\n");
				printf("Establishing connection...\n");
				char SERVER_IP[20] = {0};
				strncpy(SERVER_IP,location,20);
			
				struct sockaddr_in address;
        			int sock = 0, valread;
        			struct sockaddr_in serv_addr;
				if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        			{
                			printf("\n Socket creation error \n");
                			return NULL;
        			}
				memset(&serv_addr, '0', sizeof(serv_addr));

        			serv_addr.sin_family = AF_INET;         // Address family
        			serv_addr.sin_port = htons(SPORT);       // Server port in network byte order
				if(inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr)<=0)
        			{
                			printf("\nInvalid address/ Address not supported \n");
                			return NULL;
        			}
				if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        			{
                			printf("\nConnection Failed \n");
                			return NULL;
        			}
				send(sock, requested_file, strlen(requested_file), 0);
				printf("File request sent...\n");
				char response[1024];
				read(sock, response, 1024);
				printf("File size recieved: %s\n",response);
				char* file = malloc(sizeof(char)*strtol(response,NULL,10));
				printf("Waiting for file...\n");
				read(sock, file, strtol(response,NULL,10));
				printf("File recieved\n");
				char output_path[40] = {0};
				strcpy(output_path,"./");
				strcat(output_path,filedirname);
				strcat(output_path,"/");
				strcat(output_path,requested_file);
				FILE* output = fopen(output_path,"w+");
				if(!output){
					printf("Do you have permission here?\n");
					return NULL;
				}
				strtok(file,"\n");
				fwrite(file,1,strtol(response,NULL,10),output); 
				printf("File written to %s, closing connection\n",output_path);
				send(sock, NULL, 0, 0);
				fclose(output);
			}
			else{
				printf("You have that file\n");
			}
		}
		}
		
	}
	return NULL;
}

void * server(void* arg){
	char*** ip_array_p = arg;
	char** ip_array = *ip_array_p;

	int fd;
 	struct ifreq ifr;
 	fd = socket(AF_INET, SOCK_DGRAM, 0);
 	ifr.ifr_addr.sa_family = AF_INET;
 	strncpy(ifr.ifr_name, "eno1", IFNAMSIZ-1); 
 	ioctl(fd, SIOCGIFADDR, &ifr);
 	close(fd);
 	char* my_ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
	
	printf(">>File Server\n");
	
	int opt = 1;   
    	int master_socket , addrlen , new_socket , client_socket[3] ,  
        	max_clients = 3 , activity, i , valread , sd;   
    	int max_sd;   
    	struct sockaddr_in address;   
         
    	char buffer[1025];  //data buffer of 1K  
         
    	//set of socket descriptors  
    	fd_set readfds;

	//initialise all client_socket[] to 0 so not checked  
    	for (i = 0; i < max_clients; i++)   
    	{   
        	client_socket[i] = 0;   
    	}  
	//create a master socket  
    	if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)   
    	{   
        	perror(">>socket failed");   
        	exit(EXIT_FAILURE);   
    	}
    	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,  
          	sizeof(opt)) < 0 )   
    	{   
        	perror(">>setsockopt");   
        	exit(EXIT_FAILURE);   
    	}
	//type of socket created  
    	address.sin_family = AF_INET;   
    	address.sin_addr.s_addr = INADDR_ANY;   
    	address.sin_port = htons( SPORT );

    	if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)   
    	{   
        	perror(">>bind failed");   
        	exit(EXIT_FAILURE);   
    	}   
    	printf(">>Listener on port %d \n", SPORT);

	//try to specify maximum of 3 pending connections for the master socket  
    	if (listen(master_socket, 3) < 0)   
    	{   
        	perror(">>listen");   
        	exit(EXIT_FAILURE);   
    	}

	//accept the incoming connection  
    	addrlen = sizeof(address);   
   	puts(">>Waiting for file requests ...\n");

	
	while(1){
		//clear the socket set  
        	FD_ZERO(&readfds);   
     
        	//add master socket to set  
        	FD_SET(master_socket, &readfds);   
        	max_sd = master_socket; 
		//add child sockets to set  
        	for ( i = 0 ; i < max_clients ; i++)   
        	{   
            		//socket descriptor  
            		sd = client_socket[i];   
                 
            		//if valid socket descriptor then add to read list  
            		if(sd > 0)   
                		FD_SET( sd , &readfds);   
                 
            		//highest file descriptor number, need it for the select function  
            		if(sd > max_sd)   
                		max_sd = sd;   
        	}
		//wait for an activity on one of the sockets , timeout is NULL ,  
        	//so wait indefinitely  
        	activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);   
       
        	if ((activity < 0) && (errno!=EINTR))   
        	{   
            		printf("select error");   
        	}
			
        	if (FD_ISSET(master_socket, &readfds))   
        	{   
            		if ((new_socket = accept(master_socket,  
                    		(struct sockaddr *)&address, (socklen_t*)&addrlen))<0)   
            		{   
                		perror(">>accept");   
                		exit(EXIT_FAILURE);   
            		}   
              
                 
			//add new socket to array of sockets  
            		for (i = 0; i < max_clients; i++)   
            		{   
                		//if position is empty  
                		if( client_socket[i] == 0 )   
                		{   
                    			client_socket[i] = new_socket;    
                    			break;   
                		}   
            		}
    
		}
        	for (i = 0; i < max_clients; i++)   
        	{   
            		sd = client_socket[i];   
                 
            		if (FD_ISSET( sd , &readfds))   
            		{   
                    		//Check if it was for closing , and also read the  
                		//incoming message  
                		if ((valread = read( sd , buffer, 1024)) == 0)   
                		{	   
                    			close( sd );   
                    			client_socket[i] = 0;   
                		}   
                     
                		else 
                		{   
                    			//set the string terminating NULL byte on the end  
                    			//of the data read
					char path[40]={0};
	
					char* filedirname="";
					if(strcmp(my_ip,ip_array[1])==0)
						filedirname = "files2";
					else if(strcmp(my_ip,ip_array[0])==0)
						filedirname = "files1";
					else if(strcmp(my_ip,ip_array[2])==0)
						filedirname = "files3";
					else if(strcmp(my_ip,ip_array[3])==0)
						filedirname = "files4";
                    			
					strncpy(path,"./",2);
					strcat(path,filedirname);
					strcat(path,"/"); 
					struct stat file_stat;
					buffer[valread] = '\0'; 
					printf("\n>>File requested: %s\n>>Fetching file...\n",buffer);
					strcat(path,buffer);
					printf(">>%s\n",path);
					fd = open(path, O_RDONLY);
					if(fd==-1){
						printf(">>Failed to open file: %s",buffer);
					}
					if(fstat(fd, &file_stat)<0){
						printf(">>Error in fstat\n");
					}
					int fsize = file_stat.st_size-1;
					char size_buf[40];
					sprintf(size_buf,"%d",fsize);
					printf(">>Sending file size of: %d\n",fsize);
					send(sd,size_buf,40,0);
					char file_buf[fsize];
					read(fd,file_buf,fsize);
					send(sd,file_buf,fsize,0);
					printf("File sent!");
					close(fd);
 	               		}			   
            		}   
 		}
	}
	
	return NULL;
}

int main(int argc, char const *argv[]){
	char** ip_array = parse_hostfile("hostfile");
	pthread_t thread_id_server, thread_id_client;
	pthread_create(&thread_id_server, NULL, server, &ip_array);
	pthread_create(&thread_id_client, NULL, client, &ip_array);
	pthread_exit(NULL);
	return 0;
}
