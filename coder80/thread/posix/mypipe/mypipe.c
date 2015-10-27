#include <stdio.h>
#include <pthread.h>
#include "mypipe.h"

struct mypipe_st
{
	char data[PIPESIZE];
	int head;
	int tail;
	int datasize;
	int count_rd;
	int count_wr;
	pthread_mutex_t mut;
	pthread_cond_t cond;
};


mypipe_t *mypipe_init()
{

	struct mypipe_st *me;

	me = malloc(sizeof(*me));
	if(me == NULL)
		return NULL;

	me->head = 0;
	me->tail = 0;
	me->datasize = 0;
	me->count_rd = 0;
	me->count_wr = 0;
	pthread_mutex_init(&me->mut,NULL);
	pthread_cond_init(&me->cond,NULL);

	return me;
}

int mypipe_register(mypipe_t *ptr,int opmap);
{
	struct mypipe_st *me = ptr;
	/*if error -1*/	

	pthread_mutex_lock(&me->mut);	

	if(opmap & MYPIPE_READ)
		me->count_rd ++;
	if(opmap & MYPIPE_WRITE)
		me->count_wr ++;

	pthread_cond_broadcast(&me->cond);
	while(me->count_rd <= 0 || me->count_wr <= 0)
		pthread_cond_wait(&me->cond,&me->mut);	
	
	pthread_mutex_unlock(&me->mut);

	return 0;
}

int mypipe_unregister(mypipe_t *ptr,int opmap)
{
	struct mypipe_st *me = ptr;
    /*if error -1*/

    pthread_mutex_lock(&me->mut);

    if(opmap & MYPIPE_READ)
        me->count_rd --; 
    if(opmap & MYPIPE_WRITE)
        me->count_wr --;

	pthread_cond_broadcast(&me->cond);
	pthread_mutex_unlock(&me->mut);

	return 0;
}

static next(int a)
{
	if(a+1 == PIPESIZE)
		return 0;
	return a+1;
}

static int mypipe_readbyte(struct mypipe_st *me,char *datap)
{
	if(me->datasize <= 0)
		return -1;	

	*datap = me->data[me->head];
	me->head = next(me->head);
	me->datasize--;
	return 0;
}

ssize_t mypipe_read(mypipe_t *ptr, void *buf, size_t count)
{
	struct mypipe_st *me = ptr;	

	pthread_mutex_lock(&me->mut);	
	
	while(me->datasize <= 0 && me->count_wr > 0)
		pthread_cond_wait(&me->cond,&me->mut);

	if(me->datasize <= 0 && me->count_wr <= 0)
	{
		pthread_mutex_unlock(&me->mut);
		return 0;
	}
	for(i = 0 ; i < count; i++)
		if(mypipe_readbyte(me,buf+i) != 0)
			break;

	pthread_cond_broadcast(&me->cond);
	pthread_mutex_unlock(&me->mut);

	return i;
}





