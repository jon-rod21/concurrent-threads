#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct student_c student_t;
typedef struct pqueue_c pqueue_t;


struct pqueue_t{
    student_t *first;
    student_t *last;
    int size;
};

struct student_t{
    int id;
    int priority;
    int arrival;
    sem_t *personal_sem;
};

struct tutor_t{
    int id;
    int arrival;
    sem_t *personal_sem;
};

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        printf("improper arguments\n");
        return -1;

    }
//    int students, tutors, chairs, help;

    for (int i = 1; i < argc; i++)
    {
        printf("%s", argv[i]);
    }

    return 0;

}

