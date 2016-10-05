/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2016, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
/* <DESC>
 * Show the required mutex callback setups for GnuTLS and OpenSSL when using
 * libcurl multi-threaded.
 * </DESC>
 */
/* A multi-threaded example that uses pthreads and fetches 4 remote files at
 * once over HTTPS. The lock callbacks and stuff assume OpenSSL or GnuTLS
 * (libgcrypt) so far.
 *
 * OpenSSL docs for this:
 *   https://www.openssl.org/docs/crypto/threads.html
 * gcrypt docs for this:
 *   https://gnupg.org/documentation/manuals/gcrypt/Multi_002dThreading.html
 */

#define USE_OPENSSL /* or USE_GNUTLS accordingly */

#include <stdio.h>
#include <pthread.h>
#include <curl/curl.h>

#define NUMT 4

/* we have this global to let the callback get easy access to it */
static pthread_mutex_t *lockarray;

#ifdef USE_OPENSSL
#include <openssl/crypto.h>
static void lock_callback(int mode, int type, const char *file, int line)
{
  (void)file;
  (void)line;
  if(mode & CRYPTO_LOCK) {
    pthread_mutex_lock(&(lockarray[type]));
  }
  else {
    pthread_mutex_unlock(&(lockarray[type]));
  }
}

static unsigned long thread_id(void)
{
  unsigned long ret;

  ret=(unsigned long)pthread_self();
  return ret;
}

static void init_locks(void)
{
  int i;

  lockarray=(pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() *
                                            sizeof(pthread_mutex_t));
  for(i=0; i<CRYPTO_num_locks(); i++) {
    pthread_mutex_init(&(lockarray[i]), NULL);
  }

  CRYPTO_set_id_callback((unsigned long (*)())thread_id);
  CRYPTO_set_locking_callback(lock_callback);
}

static void kill_locks(void)
{
  int i;

  CRYPTO_set_locking_callback(NULL);
  for(i=0; i<CRYPTO_num_locks(); i++)
    pthread_mutex_destroy(&(lockarray[i]));

  OPENSSL_free(lockarray);
}
#endif

#ifdef USE_GNUTLS
#include <gcrypt.h>
#include <errno.h>

GCRY_THREAD_OPTION_PTHREAD_IMPL;

void init_locks(void)
{
  gcry_control(GCRYCTL_SET_THREAD_CBS);
}

#define kill_locks()
#endif
