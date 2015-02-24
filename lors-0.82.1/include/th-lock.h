/*!
 * @file th-lock.h
 * @brief 
 *
 * @author
 * @date 02/02/2013
 * @version
 * @details 
 *
 */
#ifndef TH_LOCK_H
/*!
 * @def TH_LOCK__H
 *    Make sure not to include this file more than once.
 */
#define TH_LOCK_H

#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self()

#include <pthread.h>
#include <openssl/crypto.h>

struct CRYPTO_dynlock_value { 
   MUTEX_TYPE mutex; 
};

/*!
 * @brief
 *
 * @fn locking_function(int mode, int n, const char *file, int line)
 * @param mode 
 * @param n
 * @param file
 * @param line
 *
 * @details [detailed description]
 */
void locking_function(int mode, int n, const char *file, int line);

/*!
 * @brief
 *
 * @fn id_function()
 * @return Returns an unsigned long.
 *
 * @details [detailed description]
 */
unsigned long id_function();

/*!
 * @brief
 *
 * @fn CRYPTO_thread_setup()
 * @return Returns an integer.
 *
 * @details [detailed description]
 */
int CRYPTO_thread_setup();

/*!
 * @brief
 *
 * @fn CRYPTO_thread_cleanup()
 *
 * @details [detailed description]
 */
void CRYPTO_thread_cleanup();

#endif // TH_LOCK_H
