#include <stdio.h>
#include <pthread.h>


typedef struct{
    int value;
    queue_t *q;
} semaphore, sem_t;

typedef struct{
    int id;
    int priority;
    int arrival;
    sem_t personal_sem;
} Student;

typedef struct{
    int id;
    int arrival;
    sem_t personal_sem;
} Tutor;

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        printf("improper arguments");
        return -1;

    }
    int students, tutors, chairs, help;

    for (int i = 1; i < argc; i++)
    {
        printf("%s", argv[i]);
    }

    return 1;

}

