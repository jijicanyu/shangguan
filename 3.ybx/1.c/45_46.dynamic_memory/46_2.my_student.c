#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NAMESIZE 1024

typedef struct student_st{
	int id;
	char *name;
} STU;

typedef struct list_st{
	int num;
	STU *p;
}LIST;


void stu_show(LIST * list_p)
{
	int i;
	for (i = 0; i < list_p->num; i++) {
		printf("%d %s\n", list_p->p[i].id, list_p->p[i].name);
	}
}

void menu()
{
	printf("\n1.add 2.del 3.edit 4.show all\n");
}

void stu_changename(STU *stu, const char *newname){
	strcpy(stu->name, newname);
}

void delete(LIST *list, int choice_id)
{
	int i;
	for (i = 0; i < list->num; i++) {
		if ((list->p+i)->id == choice_id) {
			list->num--;
			if (list->num==0) {
				free(list->p->name);
				free(list->p);
				list->p = NULL;
			}else if (i == 0) {
				free(list->p->name);
				memmove(list->p, list->p+1, sizeof(STU)*(list->num));
				list->p= realloc(list->p, (list->num)* sizeof(STU));
			}else if(i == list->num) {
				free(list->p[i].name);
				list->p = realloc(list->p, (list->num)* sizeof(STU));
			}else{
				free(list->p[i].name);
				memmove(list->p+i, list->p+i+1, 
						(list->num-i)*sizeof(STU));
				list->p= realloc(list->p, (list->num)* sizeof(STU));
			}
			break;
		}
	}
}

void edit(LIST *list, int choice_id){
	char newname[NAMESIZE];
	int i;
	for (i = 0; i < list->num; i++) {
		if ((list->p+i)->id == choice_id) {
			free(list->p[i].name);
			scanf("%s", newname);
			list->p[i].name = malloc(strlen(newname)+1);
			strcpy(list->p[i].name, newname);
			//scanf("%s", (list->p+i)->name );
			break;
		}
	}
}

int main(int argc, const char *argv[])
{
	LIST list;
	int choice, choice_id;
	char *newname;
	int ret;

	list.num = 0;
	list.p = NULL;

	newname = malloc(NAMESIZE);

	while (1) {
		menu();
		ret = scanf("%d", &choice);
		if (!ret) {
			break;
		}
		switch(choice){
			case 1://add
				list.num++;
				list.p = realloc(list.p, list.num* sizeof(STU));
				printf("plz enter for [id name]:\n");
				//scanf("%d%s", &((list.p+list.num-1)->id), (list.p+list.num-1)->name);
				scanf("%d%s", &((list.p+list.num-1)->id), newname);
				(list.p+list.num-1)->name = malloc(strlen(newname)+1);
				strcpy((list.p+list.num-1)->name, newname);
				break;

			case 2://delete
				printf("ple enter id:\n");
				ret = scanf("%d", &choice_id);
				if (ret != 1) {
					break;
				}
				delete(&list, choice_id);
				break;

			case 3:
				printf("ple enter id:\n");
				ret = scanf("%d", &choice_id);
				if (ret != 1) {
					break;
				}
				edit(&list, choice_id);
				break;
			case 4:
				//tmp.id =123;
				stu_show(&list);
				//stu_show(&tmp);
				break;
		}
	}


	return 0;
}
