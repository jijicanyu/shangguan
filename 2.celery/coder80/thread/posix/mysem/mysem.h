#ifndef MYSEM_H__
#define MYSEM_H__

typedef void mysem_t;

mysem_t *mysem_init(int initval);

int mysem_sub(mysem_t *,int n);

int mysem_add(mysem_t *,int n);

void mysem_destroy(mysem_t *);


#endif



