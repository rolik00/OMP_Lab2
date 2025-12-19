/*
 * test_pthread_rwlock.c - Тест связного списка с библиотечным pthread_rwlock_t
 * 
 * Программа тестирует производительность операций над связным списком
 * (поиск, вставка, удаление) с использованием стандартной библиотечной
 * реализации pthread_rwlock_t.
 * 
 * Использование: ./test_pthread <thread_count>
 * 
 * Вход (stdin):
 *   - Количество ключей для начальной вставки
 *   - Общее количество операций
 *   - Процент операций поиска (0-1)
 *   - Процент операций вставки (0-1)
 * 
 * Выход: время выполнения и статистика операций
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "my_rand.h"
#include "timer.h"

/* Максимальное значение ключа */
const int MAX_KEY = 100000000;

/* Узел связного списка */
struct list_node_s {
    int    data;
    struct list_node_s* next;
};

/* Глобальные переменные */
struct list_node_s* head = NULL;   /* Голова списка */
int         thread_count;           /* Число потоков */
int         total_ops;              /* Общее число операций */
double      insert_percent;         /* Процент вставок */
double      search_percent;         /* Процент поисков */
double      delete_percent;         /* Процент удалений */

/* Библиотечный rwlock и мьютекс для счётчиков */
pthread_rwlock_t    rwlock;
pthread_mutex_t     count_mutex;

/* Счётчики операций */
int member_count = 0;
int insert_count = 0;
int delete_count = 0;

/* Прототипы функций */
void  Usage(char* prog_name);
void  Get_input(int* inserts_in_main_p);
void* Thread_work(void* rank);

int   Insert(int value);
int   Member(int value);
int   Delete(int value);
void  Print(void);
void  Free_list(void);
int   Is_empty(void);

/*-----------------------------------------------------------------*/
int main(int argc, char* argv[]) {
    long i; 
    int key, success, attempts;
    pthread_t* thread_handles;
    int inserts_in_main;
    unsigned seed = 1;
    double start, finish;

    if (argc != 2) Usage(argv[0]);
    thread_count = strtol(argv[1], NULL, 10);

    Get_input(&inserts_in_main);

    /* Начальное заполнение списка (до запуска потоков) */
    i = attempts = 0;
    while (i < inserts_in_main && attempts < 2 * inserts_in_main) {
        key = my_rand(&seed) % MAX_KEY;
        success = Insert(key);
        attempts++;
        if (success) i++;
    }
    printf("Inserted %ld keys in empty list\n", i);

#ifdef OUTPUT
    printf("Before starting threads, list = \n");
    Print();
    printf("\n");
#endif

    /* Инициализация синхронизации */
    thread_handles = (pthread_t*)malloc(thread_count * sizeof(pthread_t));
    pthread_mutex_init(&count_mutex, NULL);
    pthread_rwlock_init(&rwlock, NULL);

    /* Запуск потоков и замер времени */
    GET_TIME(start);
    for (i = 0; i < thread_count; i++)
        pthread_create(&thread_handles[i], NULL, Thread_work, (void*) i);

    for (i = 0; i < thread_count; i++)
        pthread_join(thread_handles[i], NULL);
    GET_TIME(finish);

    /* Вывод результатов */
    printf("Elapsed time = %e seconds\n", finish - start);
    printf("Total ops = %d\n", total_ops);
    printf("member ops = %d\n", member_count);
    printf("insert ops = %d\n", insert_count);
    printf("delete ops = %d\n", delete_count);

#ifdef OUTPUT
    printf("After threads terminate, list = \n");
    Print();
    printf("\n");
#endif

    /* Очистка */
    Free_list();
    pthread_rwlock_destroy(&rwlock);
    pthread_mutex_destroy(&count_mutex);
    free(thread_handles);

    return 0;
}

/*-----------------------------------------------------------------*/
void Usage(char* prog_name) {
    fprintf(stderr, "usage: %s <thread_count>\n", prog_name);
    exit(0);
}

/*-----------------------------------------------------------------*/
void Get_input(int* inserts_in_main_p) {
    printf("How many keys should be inserted in the main thread?\n");
    scanf("%d", inserts_in_main_p);
    printf("How many ops total should be executed?\n");
    scanf("%d", &total_ops);
    printf("Percent of ops that should be searches? (between 0 and 1)\n");
    scanf("%lf", &search_percent);
    printf("Percent of ops that should be inserts? (between 0 and 1)\n");
    scanf("%lf", &insert_percent);
    delete_percent = 1.0 - (search_percent + insert_percent);
}

/*-----------------------------------------------------------------*/
/* Вставка значения в отсортированный список */
int Insert(int value) {
    struct list_node_s* curr = head;
    struct list_node_s* pred = NULL;
    struct list_node_s* temp;
    int rv = 1;
    
    while (curr != NULL && curr->data < value) {
        pred = curr;
        curr = curr->next;
    }

    if (curr == NULL || curr->data > value) {
        temp = (struct list_node_s*)malloc(sizeof(struct list_node_s));
        temp->data = value;
        temp->next = curr;
        if (pred == NULL)
            head = temp;
        else
            pred->next = temp;
    } else {
        rv = 0;  /* Значение уже в списке */
    }

    return rv;
}

/*-----------------------------------------------------------------*/
void Print(void) {
    struct list_node_s* temp;

    printf("list = ");
    temp = head;
    while (temp != NULL) {
        printf("%d ", temp->data);
        temp = temp->next;
    }
    printf("\n");
}

/*-----------------------------------------------------------------*/
/* Поиск значения в списке */
int Member(int value) {
    struct list_node_s* temp;

    temp = head;
    while (temp != NULL && temp->data < value)
        temp = temp->next;

    if (temp == NULL || temp->data > value) {
        return 0;
    } else {
        return 1;
    }
}

/*-----------------------------------------------------------------*/
/* Удаление значения из списка */
int Delete(int value) {
    struct list_node_s* curr = head;
    struct list_node_s* pred = NULL;
    int rv = 1;

    while (curr != NULL && curr->data < value) {
        pred = curr;
        curr = curr->next;
    }
    
    if (curr != NULL && curr->data == value) {
        if (pred == NULL) {
            head = curr->next;
        } else {
            pred->next = curr->next;
        }
        free(curr);
    } else {
        rv = 0;  /* Значение не найдено */
    }

    return rv;
}

/*-----------------------------------------------------------------*/
void Free_list(void) {
    struct list_node_s* current;
    struct list_node_s* following;

    if (Is_empty()) return;
    
    current = head; 
    following = current->next;
    while (following != NULL) {
        free(current);
        current = following;
        following = current->next;
    }
    free(current);
}

/*-----------------------------------------------------------------*/
int Is_empty(void) {
    return (head == NULL) ? 1 : 0;
}

/*-----------------------------------------------------------------*/
/* Функция потока - выполняет операции над списком */
void* Thread_work(void* rank) {
    long my_rank = (long) rank;
    int i, val;
    double which_op;
    unsigned seed = my_rank + 1;
    int my_member_count = 0, my_insert_count = 0, my_delete_count = 0;
    int ops_per_thread = total_ops / thread_count;

    for (i = 0; i < ops_per_thread; i++) {
        which_op = my_drand(&seed);
        val = my_rand(&seed) % MAX_KEY;
        
        if (which_op < search_percent) {
            /* Операция поиска - блокировка на чтение */
            pthread_rwlock_rdlock(&rwlock);
            Member(val);
            pthread_rwlock_unlock(&rwlock);
            my_member_count++;
        } else if (which_op < search_percent + insert_percent) {
            /* Операция вставки - блокировка на запись */
            pthread_rwlock_wrlock(&rwlock);
            Insert(val);
            pthread_rwlock_unlock(&rwlock);
            my_insert_count++;
        } else {
            /* Операция удаления - блокировка на запись */
            pthread_rwlock_wrlock(&rwlock);
            Delete(val);
            pthread_rwlock_unlock(&rwlock);
            my_delete_count++;
        }
    }

    /* Обновление глобальных счётчиков */
    pthread_mutex_lock(&count_mutex);
    member_count += my_member_count;
    insert_count += my_insert_count;
    delete_count += my_delete_count;
    pthread_mutex_unlock(&count_mutex);

    return NULL;
}
