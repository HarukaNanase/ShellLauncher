/*
 * list.c - implementation of the integer list functions 
 */


#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "list.h"



list_t* lst_new()
{
   list_t *list;
   list = (list_t*) malloc(sizeof(list_t));
   list->first = NULL;
   return list;
}


void lst_destroy(list_t *list)
{
	struct lst_iitem *item, *nextitem;

	item = list->first;
	while (item != NULL){
		nextitem = item->next;
		free(item);
		item = nextitem;
	}
	free(list);
}


void insert_new_process(list_t *list, int pid, time_t starttime)
{
	lst_iitem_t *item;

	item = (lst_iitem_t *) malloc (sizeof(lst_iitem_t));
	item->pid = pid;
	item->starttime = starttime;
	item->status = 0;
	item->endtime = 0;
	item->next = list->first;
	list->first = item;
}


void update_terminated_process(list_t *list, int pid, time_t endtime, int status)
{
	lst_iitem_t *item;
	item = list->first;
	
   	while(item != NULL){
		if(pid == item->pid){
				item->status = status;
				item->endtime = endtime;
				
		}
		item = item->next;
	}
}


void lst_print(list_t *list)
{
	lst_iitem_t *item;
	int Temporizador = 0;

	printf("Process list with PID and execution time in seconds:\n");
	item = list->first;
	/* while(1){ */ /* use it only to demonstrate gdb potencial */
	while (item != NULL){
		printf("%d\t%ti seconds \n", item->pid, (item->endtime - item->starttime));
		Temporizador += (item->endtime - item->starttime);
		item = item->next;
	}
	printf("Tempo total decorrido: %d segundos.\n", Temporizador);
	printf("-- end of list.\n");
}


time_t return_time(list_t *list, int pid){
	lst_iitem_t *item;
	item = list->first;
	while(item != NULL){
		if(pid == item->pid){
			return item->starttime;
		}
		item= item->next;
	}
	return -1;
}