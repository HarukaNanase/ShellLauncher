#include <stdio.h>
#include <string.h>
#include "commandlinereader.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "list.h"
#include <wait.h>
#include <semaphore.h>
#include <fcntl.h>

#define MAXSIZE 100
#define LINESIZE 100
#define PIDSIZE 100

char pipe_name[MAXSIZE];
char mypid[PIDSIZE];
int logout;

void siginthandler(int sig){
			printf("\nLogging out by SIGINT\n");

			if(strlen(mypid) == 0){
				perror("Error");
				exit(EXIT_FAILURE);
			}
			
			snprintf(mypid,PIDSIZE,"%d",getpid());
			
			printf("\nLogging out...\n");
			
			if(write(logout,mypid,strlen(mypid))<0){
				perror("Error writting to logout.");
				exit(EXIT_FAILURE);
			}
			
			printf("Logged out! Cleaning up stuff.\n");
			
			unlink(pipe_name);

			exit(0);
}


int main(int argc, char **argv){
	int fd,fd2,secret,serverstatus,clientnumber = 0;
	int MAXCLIENTS = 0;
	char numberofclients[MAXSIZE];
	char userInput[MAXSIZE];
	char static pipe_template[] = "/tmp/pipe_XXXXXX";
	char messageread[MAXSIZE];

	signal(SIGINT,siginthandler);
	strcpy(pipe_name,pipe_template);
	mktemp(pipe_name);
	char pipeligacao[MAXSIZE];
	if(argc <= 1){
		perror("No sufficient argument.");
		exit(EXIT_FAILURE);
	}
	if(argv[1] == NULL){
		perror("No argument for pipe name");
		exit(EXIT_FAILURE);
	}
	strcpy(pipeligacao,"/tmp/");
	strcat(pipeligacao,argv[1]);

	if(mkfifo(pipe_name,S_IRUSR | S_IWUSR)!= 0){
		perror("ERRO!!!!!");
		exit(EXIT_FAILURE);
	}
	
	if((serverstatus = open("/tmp/server-status",O_WRONLY))<0){
		perror("Error opening server-status pipe");
		exit(EXIT_FAILURE);
	}

	if((write(serverstatus,pipe_name,strlen(pipe_name))<0)){
		perror("Error writting to status pipe");
		exit(EXIT_FAILURE);
	}

	close(serverstatus);

	if((fd2 = open(pipe_name,O_RDONLY))<0){
		perror("Error opening pipe_name");
		exit(EXIT_FAILURE);
	}
	printf("Ready to receive information from the server.\n");
	if(read(fd2,numberofclients,MAXSIZE)<0){
		perror("Error reading server status");
		exit(EXIT_FAILURE);
	}
	close(fd2);
	sscanf(numberofclients,"%d/%d",&clientnumber,&MAXCLIENTS);
	
	if (clientnumber >= MAXCLIENTS){
		printf("Server is currently full. Please try again later!\n");
		printf("Server capacity: %d/%d\n",MAXCLIENTS,MAXCLIENTS);
		exit(0);
	}
	printf("Server capacity before logging in: %d/%d\n",clientnumber,MAXCLIENTS);
	printf("Server capacity after logging in: %d/%d\n",clientnumber+1,MAXCLIENTS);
	if((secret = open("/tmp/secret-tunnel",O_WRONLY))<0){
		perror("Error opening secret-tunnel");
		exit(EXIT_FAILURE);
	}

	if((logout = open("/tmp/logout-pipe",O_WRONLY))<0){
		perror("Error opening logout-pipe");
		exit(EXIT_FAILURE);
	}
	
	int pid = getpid();
	snprintf(mypid,PIDSIZE,"%d",pid);

	printf("Mypid %s\n",mypid);

	int n_write;

	if((n_write = write(secret,mypid,strlen(mypid)))<0){
		perror("Error writting to login");
		exit(EXIT_FAILURE);
	}
	
	printf("A ligar ao servidor...\n");
	printf("Ligado!\n");
	while(1){

		if((fd = open(pipeligacao,O_WRONLY)) < 0){
			perror("Error");
			exit(EXIT_FAILURE);
		}
		printf("Introduza o seu comando:");
		fgets(userInput,100,stdin);
		fflush(stdin);
		if(strcmp(userInput,"stats\n") == 0){
			int len = strlen(userInput);

			userInput[len-1] = ' ';
			strcat(userInput,pipe_name);
			len = strlen(userInput);
			userInput[len] = '\n';
			
			if((write(fd,userInput,strlen(userInput))<0)){
				perror("Error");
				exit(EXIT_FAILURE);
			}				
			

			if((fd2 = open(pipe_name,O_RDONLY))<0){
				perror("Error:");
				exit(EXIT_FAILURE);
			}
			
			if((read(fd2,messageread,MAXSIZE)) < 0){
				perror("Error:");
				exit(EXIT_FAILURE);
			}
			printf("Lido: %s\n",messageread);
		
			if((close(fd2)) <0){
				perror("Error");
				exit(EXIT_FAILURE);
			}

			if((close(fd))<0){
				perror("Error");
				exit(EXIT_FAILURE);
			}
		}
		else if(strcmp(userInput,"exit\n") == 0){

			snprintf(mypid,PIDSIZE,"%d",pid);
			
			printf("Logging out...\n");
			
			if(write(logout,mypid,strlen(mypid))<0){
				perror("Error writting to log-out pipe");
				exit(EXIT_FAILURE);
			}
			
			printf("Logged out! Cleaning up stuff.\n");
			
			unlink(pipe_name);
			
			exit(0);
		}

		else{
		
			if((write(fd,userInput,strlen(userInput)))<0){
				perror("Error");
				exit(EXIT_FAILURE);
			}
		
			printf("Sent %s",userInput);
		
			if((close(fd)) < 0){
				perror("Error");
				exit(EXIT_FAILURE);
			}

		}

}
}