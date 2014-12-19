#include "file_cache.h"
#define NUM_OF_THREADS 3

pthread_mutex_t  req_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_mutex_t  cache_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
pthread_cond_t cache_cond = PTHREAD_COND_INITIALIZER;

int main() {
	
  int i;    // loop counter 
  int thr_id[NUM_OF_THREADS]; // thread IDs 
  pthread_t p_threads[NUM_OF_THREADS]; // thread's structures
  char *filename[6]={"a","a","b","c","b","a"};
 
   // create the request-handling threads 
   for (i=0; i<NUM_OF_THREADS; i++) {
	 thr_id[i] = i;
	 pthread_create(&p_threads[i], NULL, process_list, (void*)&thr_id[i]);
    }
    char operation[5] = {'p','r','w','u','d'};
   // run a loop that generates requests 
   for (i=0; i<6; i++) {
     add_request(i,filename[i%6],operation[i%5], &req_mutex, &cond_var);
     
   //pause execution for a 5 sec , to allow  other threads to run and handle some requests. 
     sleep(3);
    }
    
   // now wait till there are no more requests to process 
   sleep(5);
   printf("\n All request processed \n");
   pthread_mutex_destroy(&req_mutex);
   pthread_mutex_destroy(&cache_mutex);
   pthread_cond_destroy(&cond_var);
   pthread_cond_destroy(&cache_cond);
   return 0;

}
