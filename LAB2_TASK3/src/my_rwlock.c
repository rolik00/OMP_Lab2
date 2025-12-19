/*
 * my_rwlock.c - Собственная реализация блокировки чтения-записи
 * 
 * Реализация основана на:
 * - pthread_mutex_t для защиты внутреннего состояния
 * - pthread_cond_t для ожидания читателей и писателей
 * 
 * Политика: Writer-preference (приоритет писателей)
 * - Если есть ожидающие писатели, новые читатели блокируются
 * - Это предотвращает голодание писателей при высокой нагрузке чтения
 */

#include <stdlib.h>
#include <errno.h>
#include "my_rwlock.h"

/*
 * Инициализация rwlock
 * 
 * Инициализирует mutex и обе условные переменные,
 * обнуляет все счётчики и флаги.
 */
int my_rwlock_init(my_rwlock_t* rwlock) {
    int rc;
    
    if (rwlock == NULL) {
        return EINVAL;
    }
    
    /* Инициализация мьютекса */
    rc = pthread_mutex_init(&rwlock->mutex, NULL);
    if (rc != 0) {
        return rc;
    }
    
    /* Инициализация условной переменной для читателей */
    rc = pthread_cond_init(&rwlock->readers_cv, NULL);
    if (rc != 0) {
        pthread_mutex_destroy(&rwlock->mutex);
        return rc;
    }
    
    /* Инициализация условной переменной для писателей */
    rc = pthread_cond_init(&rwlock->writers_cv, NULL);
    if (rc != 0) {
        pthread_cond_destroy(&rwlock->readers_cv);
        pthread_mutex_destroy(&rwlock->mutex);
        return rc;
    }
    
    /* Инициализация счётчиков */
    rwlock->active_readers = 0;
    rwlock->waiting_readers = 0;
    rwlock->waiting_writers = 0;
    rwlock->writer_active = 0;
    
    return 0;
}

/*
 * Уничтожение rwlock
 * 
 * Освобождает все ресурсы pthread
 */
int my_rwlock_destroy(my_rwlock_t* rwlock) {
    int rc;
    
    if (rwlock == NULL) {
        return EINVAL;
    }
    
    rc = pthread_mutex_destroy(&rwlock->mutex);
    if (rc != 0) return rc;
    
    rc = pthread_cond_destroy(&rwlock->readers_cv);
    if (rc != 0) return rc;
    
    rc = pthread_cond_destroy(&rwlock->writers_cv);
    if (rc != 0) return rc;
    
    return 0;
}

/*
 * Получение блокировки на чтение (Read Lock)
 * 
 * Алгоритм:
 * 1. Захватить mutex
 * 2. Увеличить счётчик ожидающих читателей
 * 3. Ждать, пока:
 *    - Нет активного писателя (writer_active == 0) И
 *    - Нет ожидающих писателей (waiting_writers == 0)
 *      (политика writer-preference)
 * 4. Уменьшить счётчик ожидающих, увеличить счётчик активных
 * 5. Освободить mutex
 * 
 * Несколько читателей могут работать одновременно.
 */
int my_rwlock_rdlock(my_rwlock_t* rwlock) {
    if (rwlock == NULL) {
        return EINVAL;
    }
    
    pthread_mutex_lock(&rwlock->mutex);
    
    rwlock->waiting_readers++;
    
    /* Ждём, пока нет активного писателя и нет ожидающих писателей */
    while (rwlock->writer_active || rwlock->waiting_writers > 0) {
        pthread_cond_wait(&rwlock->readers_cv, &rwlock->mutex);
    }
    
    rwlock->waiting_readers--;
    rwlock->active_readers++;
    
    pthread_mutex_unlock(&rwlock->mutex);
    
    return 0;
}

/*
 * Получение блокировки на запись (Write Lock)
 * 
 * Алгоритм:
 * 1. Захватить mutex
 * 2. Увеличить счётчик ожидающих писателей
 * 3. Ждать, пока:
 *    - Нет активных читателей (active_readers == 0) И
 *    - Нет активного писателя (writer_active == 0)
 * 4. Уменьшить счётчик ожидающих, установить флаг активного писателя
 * 5. Освободить mutex
 * 
 * Только один писатель может работать в любой момент времени.
 * Писатель получает эксклюзивный доступ.
 */
int my_rwlock_wrlock(my_rwlock_t* rwlock) {
    if (rwlock == NULL) {
        return EINVAL;
    }
    
    pthread_mutex_lock(&rwlock->mutex);
    
    rwlock->waiting_writers++;
    
    /* Ждём, пока нет активных читателей и писателей */
    while (rwlock->active_readers > 0 || rwlock->writer_active) {
        pthread_cond_wait(&rwlock->writers_cv, &rwlock->mutex);
    }
    
    rwlock->waiting_writers--;
    rwlock->writer_active = 1;
    
    pthread_mutex_unlock(&rwlock->mutex);
    
    return 0;
}

/*
 * Освобождение блокировки (Unlock)
 * 
 * Определяет тип блокировки по состоянию:
 * - Если writer_active == 1, то это писатель
 * - Иначе это читатель (уменьшаем active_readers)
 * 
 * После освобождения:
 * - Если есть ожидающие писатели - пробуждаем одного писателя
 * - Иначе пробуждаем всех ожидающих читателей
 */
int my_rwlock_unlock(my_rwlock_t* rwlock) {
    if (rwlock == NULL) {
        return EINVAL;
    }
    
    pthread_mutex_lock(&rwlock->mutex);
    
    if (rwlock->writer_active) {
        /* Писатель освобождает блокировку */
        rwlock->writer_active = 0;
        
        /* Приоритет писателям: если есть ожидающие писатели - будим одного */
        if (rwlock->waiting_writers > 0) {
            pthread_cond_signal(&rwlock->writers_cv);
        } else if (rwlock->waiting_readers > 0) {
            /* Иначе будим всех читателей */
            pthread_cond_broadcast(&rwlock->readers_cv);
        }
    } else {
        /* Читатель освобождает блокировку */
        rwlock->active_readers--;
        
        /* Если это был последний читатель */
        if (rwlock->active_readers == 0) {
            /* Приоритет писателям */
            if (rwlock->waiting_writers > 0) {
                pthread_cond_signal(&rwlock->writers_cv);
            } else if (rwlock->waiting_readers > 0) {
                pthread_cond_broadcast(&rwlock->readers_cv);
            }
        }
    }
    
    pthread_mutex_unlock(&rwlock->mutex);
    
    return 0;
}
