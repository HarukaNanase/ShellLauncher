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

#define ERRORSTATUS 10   //if the status is invalid, we will use 10 as our ErrorStatus
#define MAXLEN 7
#define TRUE 1
#define FALSE 0
#define EXIT_COMMAND "exit-global"
#define MAXPAR 2
#define LINESIZE 100
#define MESSAGESIZE 100
#define MAXSIZE 100
#define MAXCLIENTS 50
#define PIDSIZE 100
//#define PARALEL_INCREASE "more"
		/**************************************************/
		/*                   Par-Shell                    */
		/*      A server to run simultaneous processes    */
		/*           Author - André Soares 82542          */
		/*		      Guilherme Portela 82525    		  */
		/*		      André Pereira 82540       		  */
		/**************************************************/
list_t * processList;    /* Process List */
pthread_mutex_t mainLock = PTHREAD_MUTEX_INITIALIZER;   /* Mutex initialized */
int ChildProcesses, iteracao, totalexecutiontime/*AumentarParalelo*/;            /* Shared variable for the number of children processes */
int exit_status,fd2,clientNumber=0;
pthread_cond_t Running,Limiter,userCount,userLogout;
FILE *Log;
int ClientBuffer[MAXCLIENTS];
pthread_t Task,login,logout,clientrequest;
pthread_mutex_t clientLock = PTHREAD_MUTEX_INITIALIZER;


void exitfunction(){
	pthread_mutex_lock(&mainLock);
	exit_status = 1;
	
	pthread_mutex_unlock(&mainLock);
	
	printf("Shutting down all threads...\n");
	
	if(pthread_cancel(login)){
		perror("Error canceling login thread.\n");
		exit(EXIT_FAILURE);
	}
	
	if(pthread_join(login,NULL)){
		perror("Error joining login thread.\n");
		exit(EXIT_FAILURE);
	}
	
	if(pthread_cancel(logout)){
		perror("Error shutting down logout thread in SIGINT handler");
		exit(EXIT_FAILURE);
	}

	if(pthread_join(logout,NULL)){
		perror("Error with logout thread joining in SIGINT handler");
		exit(EXIT_FAILURE);
	}

	if(pthread_cancel(clientrequest)){
		perror("Error shutting down status request server");
		exit(EXIT_FAILURE);
	}

	if (pthread_join(clientrequest,NULL)){
		perror("Error with status service thread joining.");
		exit(EXIT_FAILURE);
	}

	printf("All threads shutdown.\n");
	printf("Number of clients at exit:%d\n",clientNumber);
	
	while(clientNumber > 0){
		printf("Now killing pid: %d\n",ClientBuffer[clientNumber-1]);
	
		kill(ClientBuffer[clientNumber-1],SIGKILL);
	
		clientNumber--;
	}
	
	pthread_mutex_lock(&mainLock);
	
	if(pthread_cond_signal(&Running)){
		perror("The value cond does not refer to an initialized condition variable.\n");
		exit(EXIT_FAILURE);
		}
	pthread_mutex_unlock(&mainLock);   
	printf("Finishing all processes before exiting.\n");
	if(pthread_join(Task,NULL)){
		perror("Error with monitor task joining in SIGINT handler");
		exit(EXIT_FAILURE);
	}
	
	printf("*******************************\n*   Monitor Thread Joined !   *\n*******************************\n");
	lst_print(processList);
	if(Log == NULL){
		exit(EXIT_FAILURE);
	}
	fclose(Log);
	unlink("/tmp/secret-tunnel");
	unlink("/tmp/logout-pipe");
	unlink("/tmp/par-shell-in");
	unlink("/tmp/server-status");
	exit(EXIT_FAILURE);
}


	/********************************************/
	/*				SignalHandler				*/
	/* Handler for SigInt signal sent by cltr-c */
	/********************************************/
void signalhandler(int sig){
	printf("\nSIGINT received... now handling.\n");
	exitfunction();
	
}		

	/****************************************************/
	/*				Par-shell loginQueue				*/
	/*		     Task to login terminal client    	    */
	/*													*/
	/****************************************************/

void* loginQueue(){
	char bufferleitura[MAXSIZE];
	int clientpid;
	

	if(mkfifo("/tmp/secret-tunnel",S_IRUSR | S_IWUSR) != 0){
			perror("Error");
			exit(EXIT_FAILURE);
			}
	
	while(exit_status == 0){
		pthread_mutex_lock(&clientLock);
		printf("Current users: %d\n", clientNumber);	
		pthread_mutex_unlock(&clientLock);
		
		if((fd2 = open("/tmp/secret-tunnel",O_RDONLY))<0){
			perror("Error opening secret-tunnel");
			exit(EXIT_FAILURE);
			}
		
		int n_read;
		if((n_read=read(fd2,bufferleitura,MAXSIZE))<0){
			perror("Error while reading login.");
			exit(EXIT_FAILURE);
		}
		if(n_read == 0){
			continue;
		}
		clientpid = atoi(bufferleitura);
		
		pthread_mutex_lock(&clientLock);
		ClientBuffer[clientNumber] = clientpid;
		clientNumber++;
		pthread_mutex_unlock(&clientLock);

		printf("Client %d has logged in!\n",ClientBuffer[clientNumber - 1]);
		close(fd2);	
		}
	printf("loginQueue thread about to join...\n");
	pthread_exit(NULL);
	
}

	/******************************************/
	/*    		Par-Shell logout task		  */
	/*		Logs users out when they exit.	  */
	/*										  */	
	/******************************************/

void* logoutThread(){
	char logoutpid[PIDSIZE];
	int actual;
	int pid;
	unlink("/tmp/logout-pipe");
	if(mkfifo("/tmp/logout-pipe",S_IWUSR | S_IRUSR) != 0){
		perror("Error making logout-pipe");
		exit(EXIT_FAILURE);
	}	
	int fd5;
	while(exit_status == 0){		
		if((fd5 = open("/tmp/logout-pipe",O_RDONLY)) < 0){
			perror("Error opening logout-pipe.");
			exit(EXIT_FAILURE);
		}
		
		if(read(fd5,logoutpid,PIDSIZE) < 0){
			perror("error reading from log-out pipe");
			exit(EXIT_FAILURE);
		}
		pid = atoi(logoutpid);
	
		if(pid < 0){
			continue;
		}
		actual = 0;
		while(ClientBuffer[actual] != pid){
			actual++;
			if(actual >= clientNumber){
				continue;
			}
		}
		pthread_mutex_lock(&clientLock);
		ClientBuffer[actual] = ClientBuffer[clientNumber - 1];
		clientNumber --;		
		printf("User %d logged out.\n",pid);
		printf("Current users: %d\n",clientNumber);
		pthread_cond_signal(&userCount);
		pthread_mutex_unlock(&clientLock);
		

		close(fd5);

		}
	pthread_exit(NULL);
	}


	/**********************************************/
	/*			StatusRequest Thread			  */
	/*	   Delivers server status to terminals	  */
	/**********************************************/


void* statusRequest(){
	unlink("/tmp/server-status");
	clientNumber = 0;
	if(mkfifo("/tmp/server-status",S_IRUSR | S_IWUSR)<0){
		perror("Error on creating server-status pipe.");
		exit(EXIT_FAILURE);
	}
	int serverstatus;
	int clientfd;
	char clientpipe[MAXSIZE];
	char *numberofclients;
	numberofclients = malloc(MAXSIZE*sizeof(char));

	while(1){
		if((serverstatus = open("/tmp/server-status",O_RDONLY))<0){
			perror("Error opening server-status pipe");
			exit(EXIT_FAILURE);
		}
		
		if((read(serverstatus,clientpipe,MAXSIZE))<0){
			perror("Error getting client pid for request\n");
			exit(EXIT_FAILURE);
		}
				
		if((clientfd = open(clientpipe,O_WRONLY))<0){
			perror("Error opening clientpipe.");
			exit(EXIT_FAILURE);
		}
		
		pthread_mutex_lock(&clientLock);
		snprintf(numberofclients,MAXSIZE,"%d/%d",clientNumber,MAXCLIENTS);
		if(write(clientfd,numberofclients,strlen(numberofclients))<0){
			perror("Error delivering status to terminal");
			exit(EXIT_FAILURE);
		}
		pthread_mutex_unlock(&clientLock);
		
		close(serverstatus);
	}
}



	/****************************************************/
	/*			  Par-shell processMonitor	   			*/
	/*													*/
	/*													*/
	/****************************************************/



void* processMonitor(){
	int pid,status;
	time_t timer;

	while(TRUE){
		pthread_mutex_lock(&mainLock);
		while(ChildProcesses == 0 && exit_status == 0){
			if(pthread_cond_wait(&Running, &mainLock)){
				perror("Error on Running wait:\n");
				exit(EXIT_FAILURE);
			}
		}
		pthread_mutex_unlock(&mainLock);
		
		if((exit_status == 1) && (ChildProcesses == 0) ){
					if(pthread_cond_destroy(&Limiter)){
					
						perror("Error destroying Limiter cond.\n");
						exit(EXIT_FAILURE);
					}

					if(pthread_cond_destroy(&Running)){
						perror("Error destroying Running cond.\n");
						exit(EXIT_FAILURE);
					}	
				
					pthread_exit(NULL);
				}
		else{
			pid = wait(&status);
				
			timer = time(NULL);
			if((pid<0) && (errno == EINTR)){	
			continue;
			}
				
			if (WIFEXITED(status) == TRUE){
				status = WEXITSTATUS(status);
			}
			else{
				perror("WIFEXITED(status) returned false.");
				exit(EXIT_FAILURE);
				}
			
			pthread_mutex_lock(&mainLock);
			update_terminated_process(processList,pid,timer,status);
			
			totalexecutiontime += (int)(timer - return_time(processList,pid));
			iteracao++;

			ChildProcesses--;
			
			if(pthread_cond_signal(&Limiter)){
				perror("Error sending signal to Limiter.\n");
				exit(EXIT_FAILURE);
			}

			pthread_mutex_unlock(&mainLock);
			
			
			if(fprintf(Log,"Iteracao %d\n",iteracao) < 0){
				perror("Error writting 'Iteracao N' to file.\n");
				exit(EXIT_FAILURE);
			}

		
			if(fprintf(Log,"PID: %d execution time: %d\n",pid, (int)(timer - return_time(processList,pid))) < 0){
				perror("Error writting 'PID: pid execution time: time' to file.\n");
				exit(EXIT_FAILURE);
			}

	
			if(fprintf(Log,"total execution time: %d s\n", totalexecutiontime) < 0){
				perror("Error writting 'total execution time: total' to file.\n");
				exit(EXIT_FAILURE);
			}
		
			if(fflush(Log)){
				perror("Error flushing\n");
				exit(EXIT_FAILURE);
							

			}
		}
	}
}




int main(){
		char *argVector[MAXLEN];				 /*Array to contain user input*/
		int pid; time_t Begin;   	 /*Initialize all integer variables needed */
		char buffer[LINESIZE];
		processList = lst_new();
		int fd,args;
		unlink("/tmp/par-shell-in");
		unlink("/tmp/secret-tunnel");
		unlink("/tmp/logout-pipe");

		signal(SIGINT,signalhandler);

		if(mkfifo("/tmp/par-shell-in",S_IRUSR | S_IWUSR) != 0){
			perror("Error");
			exit(EXIT_FAILURE);
			}
		if(pthread_create(&clientrequest,NULL,statusRequest,NULL)){
			perror("Error creating status thread..");
			exit(EXIT_FAILURE);
		}

		if(pthread_create(&login,NULL,loginQueue,NULL)){
			perror("error creating login queue thread.");
			exit(EXIT_FAILURE);
		}

		if(pthread_create(&logout,NULL,logoutThread,NULL)){
			perror("error creating logout thread.");
			exit(EXIT_FAILURE);
		}
	
		printf("Awaiting first connection...\n");

		if((fd = open("/tmp/par-shell-in",O_RDONLY))<0){
			perror("error opening par-shell-in");
			exit(EXIT_FAILURE);
		}
	
		close(0);
		dup(fd);
		printf("Connection sucessful.\n");
		
		Log = fopen("log.txt","a+");
		if(Log == NULL){
			perror("Error opening file.\n");
			exit(EXIT_FAILURE);
		}

		if(fgets(buffer,LINESIZE,Log) == NULL){
			iteracao = 0;
			totalexecutiontime = 0;
			printf("*****************************************************\n*        File was empty. Set variables to 0.        *\n");
		}

		else{
			printf("*****************************************************\n*             Started to read file.                 *\n");
			while(fgets(buffer,LINESIZE,Log) != NULL){
				sscanf(buffer,"Iteracao %d",&iteracao);
				sscanf(buffer,"total execution time: %d s",&totalexecutiontime);
			}
			iteracao++;
			printf("* Reading file ended. Variables loaded from Log.txt *\n");
		}	
		
		if(pthread_cond_init(&userCount,NULL)){

			perror("Error initializing pthread_cond_t userCount.\n");
			exit(EXIT_FAILURE);
		}
		
		if(pthread_cond_init(&Limiter,NULL)){

			perror("Error initializing pthread_cond_t Limiter.\n");
			exit(EXIT_FAILURE);
		}


		
		if(pthread_cond_init(&Running,NULL)){
		
			perror("Error initializing pthread_cond_t Running.\n");
			exit(EXIT_FAILURE);
		}


		if(pthread_create(&Task,NULL,processMonitor,NULL)){
			perror("Error creating thread.\n");
			exit(EXIT_FAILURE);
		}
		printf("*           Monitor Thread Initialized!             *\n*****************************************************\n");

		
		/************************************************************************/
		/*						Ciclo de Leitura								*/
		/*																		*/
		/*																		*/
		/*																		*/
		/************************************************************************/

		while(1){
				printf("Introduza um comando:\n");
				args = readLineArguments(argVector,MAXLEN,buffer,LINESIZE);
				
				/* CASO EM QUE FICA SEM CLIENTES ! */
				if(args < 0){
					printf("No clients connected. Going to sleep.\n");
					if((fd = open("/tmp/par-shell-in",O_RDONLY))<0){
						perror("Error:");
						exit(EXIT_FAILURE);
					}
					printf("A client connected.\n");
					continue;
				}
			
				/* CASO NULL */
				
				while(args == 0){
					printf("Comando invalido.\n");
					continue;
				}

				/* CASO STATS */

				if(strcmp(argVector[0],"stats") == 0){
		
					int temp;
					if((temp = open(argVector[1],O_WRONLY,S_IRWXU)) < 0){
						perror("Error:");
						exit(EXIT_FAILURE);
						}
				
					char message[MESSAGESIZE];
					
					pthread_mutex_lock(&mainLock);
					snprintf(message,MESSAGESIZE,"Child processes: %d Total Execution Time: %d s \n",ChildProcesses,totalexecutiontime);
					pthread_mutex_unlock(&mainLock);
				
					int escrito;
					if((escrito = write(temp,message,MESSAGESIZE))<0){
						perror("Error");
						exit(EXIT_FAILURE);
					}

					printf("Bytes escritos: %i\n",escrito);
					
					if(close(temp)<0){
						perror("Error");
						exit(EXIT_FAILURE);
					}
					continue;
				}

				/* CASOS != NULL || CASOS != EXIT || CASOS != STATS */

				if(strcmp(argVector[0],EXIT_COMMAND)){	
			
			
					pthread_mutex_lock(&mainLock);
					while(ChildProcesses >= (MAXPAR)){
						if(pthread_cond_wait(&Limiter,&mainLock)){
							perror("Waiting on Limiter condition failed.\n");
							exit(EXIT_FAILURE);
							}
						}
					pthread_mutex_unlock(&mainLock);
		
					pid = fork();			
					if(pid == 0){					/*What the children process should execute starts here */					
						signal(SIGINT,SIG_IGN);
						char nomefilho[MAXSIZE];
						int mypid;
						mypid = getpid();
						snprintf(nomefilho,MAXSIZE,"par-shell-out-%d.txt",mypid);
						close(1);
						open(nomefilho,O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
						execv(argVector[0],argVector);					
						perror("Error");
						exit(EXIT_FAILURE);   			/* _exit vs exit - Round 1 */  /* exit wins - teacher advice */			
			
					}
					else if (pid < 0){
						fprintf(stderr,"Error while Forking... Please try again.\n");	/* Forking went wrong */

					}
					else{		/* What the father process does starts here,*/
				
						Begin = time(NULL);
						pthread_mutex_lock(&mainLock);				
					
						insert_new_process(processList,pid,Begin);
					
						ChildProcesses++;
						if(pthread_cond_signal(&Running)){
							perror("The value cond does not refer to an initialized condition variable.\n");
							exit(EXIT_FAILURE);
						}				
				
						pthread_mutex_unlock(&mainLock);	

						}
				}

				/* CASO EXIT */

				if((strcmp(argVector[0],EXIT_COMMAND))==0){
						exitfunction();
					}	
			}
			
		return -1;
		}

