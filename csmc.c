#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

typedef struct pq_node{
    int student_id;
    int priority;
    int arrival_order;
    struct pq_node *next;
} pq_node;

typedef struct{
    pq_node *head;
    int size;
    int next_arrival;
} pq_t;

typedef struct tutor_node{
    int tutor_id;
    struct tutor_node *next;
} tutor_node; 

typedef struct{
    tutor_node *head;
    tutor_node *tail;
    int size;
} tutor_t;


void nano_sleep(long m_sec)
{
    struct timespec ts;
    ts.tv_sec = m_sec / 1000000;
    ts.tv_nsec = (m_sec % 1000000) * 1000;
    nanosleep(&ts, NULL);
}


pq_t *waiting_students;
tutor_t *available_tutors;

pthread_mutex_t student_mut;
pthread_mutex_t tutor_mut;
pthread_mutex_t print_mut;
pthread_mutex_t chairs_mut;
pthread_mutex_t stats_mut;

sem_t student_arrived;
sem_t coordinator_ready;
sem_t *tutor_assigned;
sem_t *student_ready;

int *student_help_count;
int *tutor_current_student;
int available_chairs;
int num_chairs;
int num_students;
int num_tutors;
int help_needed;
int total_help_requested = 0;
int total_sessions_tutored = 0;
int active_tutoring_sessions = 0;
int students_finished = 0;

// Priority Queue Methods
pq_t* createPQ()
{
    pq_t *pq = (pq_t*)malloc(sizeof(pq_t));
    if (pq == NULL)
    {
        fprintf(stderr, "Failed to allocate priority queue");
        exit(1);
    }

    pq->head = NULL;
    pq->size = 0;
    pq->next_arrival = 0;

    return pq;
}


void pq_enqueue(pq_t *pq, int student_id, int priority)
{
    pq_node *new_node = (pq_node*)malloc(sizeof(pq_node));
    new_node->student_id = student_id;
    new_node->priority = priority;
    new_node->arrival_order = pq->next_arrival++;
    new_node->next = NULL;
    
    if (pq->head == NULL || 
            pq->head->priority > priority ||
            (pq->head->priority == priority && pq->head->arrival_order >
             new_node->arrival_order))
    {
        new_node->next = pq->head;
        pq->head = new_node;
    }
    else
    {
        pq_node *cur = pq->head;
        while(cur->next != NULL && 
                (cur->next->priority < priority ||
                 (cur->next->priority == priority && cur->next->arrival_order <
                  new_node->arrival_order)))
        {
            cur = cur->next;
        }
        new_node->next = cur->next;
        cur->next = new_node;
    }
    pq->size++;
}


int pq_dequeue(pq_t *pq)
{
    if (pq->head == NULL)
    {
        return -1;
    }

    pq_node *temp = pq->head;
    int student_id = temp->student_id;
    pq->head = pq->head->next;
    free(temp);
    pq->size--;

    return student_id;
}


int pq_isEmpty(pq_t *pq)
{
    return pq->head == NULL;
}

void pq_free(pq_t *pq) {
    while(pq->head != NULL)
    {
        pq_node *temp = pq->head;
        pq->head = pq->head->next;
        free(temp);
    }
    free(pq);
}
// End of PQ Methods

// Tutor Queue Methods
tutor_t* createTQ()
{
    tutor_t *tq = (tutor_t*)malloc(sizeof(tutor_t));
    tq->head = NULL;
    tq->tail = NULL;
    tq->size = 0;
    return tq;
}

void tq_enqueue(tutor_t *tq, int tutor_id)
{
    tutor_node *new_node = (tutor_node*)malloc(sizeof(tutor_node));
    new_node->tutor_id = tutor_id;
    new_node->next = NULL;

    if (tq->tail == NULL)
    {
        tq->head = tq->tail = new_node;
    }
    else
    {
        tq->tail->next = new_node;
        tq->tail = new_node;
    }
    tq->size++;
}

int tq_dequeue(tutor_t *tq)
{
    if (tq->head == NULL)
    {
        return -1;
    }
    
    tutor_node *temp = tq->head;
    int tutor_id = temp->tutor_id;
    tq->head = tq->head->next;
    
    if (tq->head == NULL)
    {
        tq->tail = NULL;
    }
    free(temp);
    tq->size--;
    return tutor_id;
}

int tq_isEmpty(tutor_t *tq)
{
    return tq->head == NULL;
}

void tq_free(tutor_t *tq)
{
    while (tq->head != NULL)
    {
        tutor_node *temp = tq->head;
        tq->head = tq->head->next;
        free(temp);
    }
    free(tq);
}
// End of TQ Methods

void* coordinator_thread(void *arg)
{
    (void)arg;

    while (1)
    {
        sem_wait(&student_arrived);

        pthread_mutex_lock(&student_mut);

        if (students_finished == num_students && pq_isEmpty(waiting_students))
        {
            pthread_mutex_unlock(&student_mut);

            for (int i = 0; i < num_tutors; i++)
            {
                tutor_current_student[i] = -2;
                sem_post(&tutor_assigned[i]);
            }
            break;
        }

        if (pq_isEmpty(waiting_students))
        {
            pthread_mutex_unlock(&student_mut);
            continue;
        }

        int student_id = pq_dequeue(waiting_students);
        pthread_mutex_unlock(&student_mut);

        if (student_id == -1)
        {
            continue;
        }

        pthread_mutex_lock(&tutor_mut);
        while (tq_isEmpty(available_tutors))
        {
            pthread_mutex_unlock(&tutor_mut);
            nano_sleep(100);
            pthread_mutex_lock(&tutor_mut);
        }
        int tutor_id = tq_dequeue(available_tutors);
        pthread_mutex_unlock(&tutor_mut);

        tutor_current_student[tutor_id] = student_id;
        sem_post(&tutor_assigned[tutor_id]);
    }
    return NULL;
}

void* tutor_thread (void *arg)
{
    int tutor_id = *(int*)arg;
    free(arg);

    while (1)
    {
        pthread_mutex_lock(&tutor_mut);
        tq_enqueue(available_tutors, tutor_id);
        pthread_mutex_unlock(&tutor_mut);

        sem_wait(&tutor_assigned[tutor_id]);

        int student_id = tutor_current_student[tutor_id];

        if(student_id == -2)
        {
            break;
        }

        if (student_id < 0)
        {
            continue;
        }

        sem_post(&student_ready[student_id]);

        pthread_mutex_lock(&stats_mut);
        active_tutoring_sessions++;
        int current_active = active_tutoring_sessions;
        total_sessions_tutored++;
        int total_tutored = total_sessions_tutored;
        pthread_mutex_unlock(&stats_mut);

        pthread_mutex_lock(&print_mut);
        printf("T: Student %d tutored by Tutor %d. Total sessions being tutored = %d. Total sessions tutored by all = %d.\n", student_id, tutor_id, current_active, total_tutored);
        pthread_mutex_unlock(&print_mut);

        nano_sleep(200);

        pthread_mutex_lock(&stats_mut);
        active_tutoring_sessions--;
        pthread_mutex_unlock(&stats_mut);
    }
    return NULL;
}

void* student_thread(void *arg)
{
    int student_id = *(int*)arg;
    free(arg);

    unsigned int seed = (unsigned int)(time(NULL) ^ pthread_self());

    for (int help = 0; help < help_needed; help++)
    {
        nano_sleep(rand_r(&seed) % 2001);

        pthread_mutex_lock(&chairs_mut);
        if (available_chairs > 0)
        {
            available_chairs--;
            int chairs_left = available_chairs;
            pthread_mutex_unlock(&chairs_mut);

            pthread_mutex_lock(&print_mut);
            printf("S: Student %d takes a seat. Empty chairs remaining = %d.\n", student_id, chairs_left);
            pthread_mutex_unlock(&print_mut);

            pthread_mutex_lock(&student_mut);
            int priority = student_help_count[student_id] + 1;
            pq_enqueue(waiting_students, student_id, priority);
            int waiting = waiting_students->size;

            pthread_mutex_lock(&stats_mut);
            total_help_requested++;
            int total_requests = total_help_requested;
            pthread_mutex_unlock(&stats_mut);

            pthread_mutex_unlock(&student_mut);
            
            pthread_mutex_lock(&print_mut);
            printf("C: Student %d with priority %d in queue. Waiting students = %d. Total help requested so far %d.\n", student_id, priority, waiting, total_requests);
            pthread_mutex_unlock(&print_mut);


            sem_post(&student_arrived);

            sem_wait(&student_ready[student_id]);

            pthread_mutex_lock(&chairs_mut);
            available_chairs++;
            pthread_mutex_unlock(&chairs_mut);
            

            pthread_mutex_lock(&print_mut);
            printf("S: Student %d receives help from Tutor.\n", student_id);
            pthread_mutex_unlock(&print_mut);

            nano_sleep(200);

            student_help_count[student_id]++;

        }
        else
        {
            pthread_mutex_unlock(&chairs_mut);

            pthread_mutex_lock(&print_mut);
            printf("S: Student %d found no empty chair. Will come again later.\n", student_id);
            pthread_mutex_unlock(&print_mut);

            help--;
        }
    }

    pthread_mutex_lock(&stats_mut);
    students_finished++;
    pthread_mutex_unlock(&stats_mut);

    sem_post(&student_arrived);
    return NULL;
}




int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        printf("invalid arguments\n");
        return -1;
    }

    num_students = atoi(argv[1]);
    num_tutors = atoi(argv[2]);
    num_chairs = atoi(argv[3]);
    help_needed = atoi(argv[4]);

    if (num_students <= 0 || num_tutors <= 0 || num_chairs <= 0 || help_needed <= 0)
    {
        fprintf(stderr, "all arguments must be positive");
        return 1;
    }

    available_chairs = num_chairs;

    waiting_students = createPQ();
    available_tutors = createTQ();
    student_help_count = (int*)calloc(num_students, sizeof(int));
    tutor_current_student = (int*)malloc(num_tutors * sizeof(int));
    student_ready = (sem_t*)malloc(num_students * sizeof(sem_t));
    tutor_assigned = (sem_t*)malloc(num_tutors * sizeof(sem_t));

    for (int i = 0; i < num_students; i++)
    {
        sem_init(&student_ready[i], 0, 0);
    }

    for (int i = 0; i < num_tutors; i++)
    {
        sem_init(&tutor_assigned[i], 0, 0);
        tutor_current_student[i] = -1;
    }

    sem_init(&student_arrived, 0, 0);
    sem_init(&coordinator_ready, 0, 0);

    pthread_mutex_init(&student_mut, NULL);
    pthread_mutex_init(&tutor_mut, NULL);
    pthread_mutex_init(&print_mut, NULL);
    pthread_mutex_init(&chairs_mut, NULL);
    pthread_mutex_init(&stats_mut, NULL);

    pthread_t coordinator; 
    pthread_t *tutors = (pthread_t*)malloc(num_tutors * sizeof(pthread_t));
    pthread_t *students = (pthread_t*)malloc(num_students * sizeof(pthread_t));

    pthread_create(&coordinator, NULL, coordinator_thread, NULL);

    for (int i = 0; i < num_tutors; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&tutors[i], NULL, tutor_thread, id);
    }

    for (int i = 0; i < num_students; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&students[i], NULL, student_thread, id);
    }

    for (int i = 0; i < num_students; i++)
    {
        pthread_join(students[i], NULL);
    }

    pthread_join(coordinator, NULL);
    for (int i = 0; i < num_tutors; i++)
    {
        pthread_join(tutors[i], NULL);
    }

    pq_free(waiting_students);
    tq_free(available_tutors);
    free(student_help_count);
    free(tutor_current_student);
    free(student_ready);
    free(tutor_assigned);
    free(tutors);
    free(students);

    pthread_mutex_destroy(&student_mut);
    pthread_mutex_destroy(&tutor_mut);
    pthread_mutex_destroy(&print_mut);
    pthread_mutex_destroy(&chairs_mut);
    pthread_mutex_destroy(&stats_mut);
    sem_destroy(&student_arrived);
    sem_destroy(&coordinator_ready);

    return 0;
}

