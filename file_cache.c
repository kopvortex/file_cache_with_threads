#include "file_cache.h"

extern pthread_mutex_t req_mutex ; 
extern pthread_mutex_t cache_mutex ;
//global conditional variable

extern pthread_cond_t cond_var ;
extern pthread_cond_t cache_cond;

file_cache* head = NULL; /* pointer to the  head of the file_cache linked list */
file_cache* tail = NULL; /* pointer to the  tail of the file_cache linked list */

int max_entries =0; // global variable to track the no of cache entries .

char *file_content ="hello, how are you"; // pointer to the data to write when a file is opened for write. 

// Defintion of a request list . Client add new request to the request list to be processed.

int n_requests = 0; /* number of pending requests, initial value 0 */


request_list *first = NULL; // pointer to the first request
request_list *last  = NULL; // pointer to the last request

//thread relatd functions

/* 
 * function name : add_request
 * arguments     : num              : Indicate request number 
                   filename         : Name of the file added to request
                   p_mutex          : Mutex variable
                   p_cond           : Conditional variable
 * purpose       : To add a request to the request list.
 * return value  : Nothing
 *
*/

void add_request(int num, char *filename, char operation, pthread_mutex_t* p_mutex, pthread_cond_t *p_cond) {
	request_list *new_req ; //pointer to new request
	new_req = (request_list*)malloc(sizeof(request_list));
	new_req->req_number = num;
	new_req->operation = operation;
	new_req->filename = filename;
	new_req->next = NULL;
	printf("\n Currently adding request for file %s \n", new_req->filename);
    // Going to add new request to global request list. To make sure exclusive access, lock mutex 
   int m_val = pthread_mutex_lock(p_mutex);
   
   if(n_requests == 0) { // There is no pending request . If so first and last point to new_req.
	   first = new_req;
	   last = new_req;
    }
    else { // If request_list not empty , add new request to end of list.
	   last->next = new_req;
	   last = new_req;	
	}		   
    n_requests++; // No of pending requst is incremented.
    #ifdef DEBUG
       printf("\n Added request with id '%d' to the reques_list \n", new_req->req_number);
       fflush(stdout);
    #endif /* DEBUG */
   // Now unlock the mutex and signal conditional variable to indicate that there is a new request to handle
   m_val = pthread_mutex_unlock(p_mutex);
   m_val = pthread_cond_signal(p_cond);
}   

/* 
 * function name : get_req
 * arguments     : p_mutex          : Mutex variable
 * purpose       : To get a request structure from request list
 * return value  : Pointer to a request_list object
 *
*/

request_list* get_req(pthread_mutex_t* p_mutex){

   request_list* new_req; // pointer to request. 
   #ifdef DEBUG
       printf("\n In function get_req() \n");
       fflush(stdout);
   #endif /* DEBUG */
// lock the mutex, to assure exclusive access to the list
   int m_val = pthread_mutex_lock(p_mutex);
   if (n_requests > 0) {
	 #ifdef DEBUG
       printf("\n Request list not empty. %d request pending to be processed \n",n_requests);
       fflush(stdout);
   #endif /* DEBUG */
	 new_req = first;
	 first = first->next;
	 if (first == NULL) { // If this was the last request on the list 
	   last = NULL;
	 }
	// decrease the total number of pending requests 
	 n_requests--;
	}
   else { // requests list is empty 
	 new_req = NULL;
   }
   // unlock mutex 
   m_val = pthread_mutex_unlock(p_mutex);
   // return the extracted request to the caller.
   return new_req;
}

/* 
 * function name : do_request
 * arguments     : new_req  : A pointer to a request_list object
 * purpose       : To process the request_list pointed by new_req & do the cache specific operation
 * return value  : Nothing
 *
*/

void do_request(request_list *new_req){
	const char *buf ;
     switch(new_req->operation){
	  case 'p'    :  printf("\n Pin files %s to cache \n",new_req->filename);
	                 file_cache_pin_files(head,&(new_req->filename),1);
	                 break;
	  case 'r'    :  printf("\n Read data from file %s from cache \n",new_req->filename);
	                 file_cache_file_data(head,new_req->filename);
	                 break;
	  case 'w' :     printf("\n Write data to cache for file %s \n",new_req->filename);
	                 file_cache_mutable_file_data(head,new_req->filename);
	                 break;
	  case 'u' :     printf("\n Unpin file %s from cache \n", new_req->filename);
	                 file_cache_unpin_files(head,&(new_req->filename),1);
	                 break; 
	  case 'd' :     printf("\n Flush all dirty cache \n");
	                 file_cache_destroy(head);
	                 break;
	  default  :     printf("\n No operation specified \n");
	                 break;                                                  
	}
	printf("\n Request processed for file %s \n",new_req->filename);	 	
}

/* 
 * function name : process_list
 * arguments     : id  : An id indicating the thread that called the function
 * purpose       : To access the request_list and send request to do_request() to process it
 * return value  : Nothing
 *
*/	
void *process_list(void *id) {

   request_list *new_req;
   int thread_id = *((int*)id);
   printf("Starting thread '%d'\n", thread_id);
   // lock mutex to access request list and process request
   int m_val = pthread_mutex_lock(&req_mutex);
   #ifdef DEBUG
   printf("thread '%d' has pthread_mutex_lock\n", thread_id);
   fflush(stdout);
   #endif /* DEBUG */
   while(1) {
      if(n_requests > 0) { // There are pending request to be processed.
	      new_req = get_req(&req_mutex);
	      if(new_req){ // If request pending unlock mutex so that other thread can process it and then free request and then lock mutex
	        pthread_mutex_unlock(&req_mutex);
		    do_request(new_req);
		    free(new_req);
		    m_val = pthread_mutex_lock(&req_mutex); 
           }
      }
      else {
	      //wait for request to arrive.
          #ifdef DEBUG
            printf("thread '%d' waiting before pthread_cond_wait\n", thread_id);
            fflush(stdout);
          #endif /* DEBUG */
          m_val = pthread_cond_wait(&cond_var, &req_mutex); 
          //mutex automatically locked after pthread_cond_wait . So need to lock seperately.
          #ifdef DEBUG
	        printf("thread '%d' after pthread_cond_wait\n", thread_id);
	        fflush(stdout);
	      #endif /* DEBUG */
      }	   
   }
}   	


/* 
 * function name  : file_cache_destroy
 * arguments      : file_cache * cache : Pointer to file_cache object
 * purpose        : Destructor. Flushes all dirty buffers to local file system. Make the buffer associated with the cache empty.
 * return value   : nothing 
*/

void file_cache_destroy(file_cache *cache){
   pthread_mutex_lock(&cache_mutex);
   while(cache){
	   if(cache->pin && cache->dirty){ // Search through each cache to check for dirty buffer.
		   write_to_file(cache);
		   #ifdef DEBUG
				printf("\n Successfully written to file %s \n",cache->filename);
				fflush(stdout);
           #endif  /* DEBUG */
	   }
	   strcpy(cache->buf,""); /*make the cache buffer empty for the nondirty node */
	   cache = cache->next;
   }
   pthread_mutex_unlock(&cache_mutex);	
}	

/* 
 * function name : search_cache
 * arguments     : file_cache* temp : Pointer to head of file_cache list, 
                   char *files      : Pointer to name of file to be searched in cache
 * purpose       : to check the desired file is present in cache or not.
 * return value  : Pointer to file_cache object associated with the file , if file is present in cache or NULL when file doesnt exist in cache.
 *
*/

file_cache* search_cache(file_cache* temp,char *files){
	pthread_mutex_lock(&cache_mutex);
	#ifdef DEBUG
				printf("\n Begin searching cache, in search_cache() \n");
				fflush(stdout);
    #endif  /* DEBUG */
    
	while(temp){      // Start iterating from the begining of the file_cache list 
	    #ifdef DEBUG
			//	printf("\n Cache file is %s\n",temp->filename);
			//	printf("\n File to be searched is %s\n",files);
				fflush(stdout);
        #endif  /* DEBUG */
		if(strcmp(temp->filename,files) == 0 ){   //Compare file name to be searched and file name in cache
			return temp;
		#ifdef DEBUG
				printf("\n File %s in cache \n",files);
				fflush(stdout);
        #endif  /* DEBUG */	
		}
		else {
		   temp = temp->next;
		}
  }
  pthread_mutex_unlock(&cache_mutex);
  return NULL;
}

/* 
 * function name : add_to_cache
 * arguments     : char *file : Pointer to name of file to be loaded to cache
 * purpose       : To add a file to the cache list
 * return value  : Pointer to file_cache object associated with the newly added file.
 *
*/

file_cache* add_to_cache(char *file){
	   pthread_mutex_lock(&cache_mutex);
	    #ifdef DEBUG
				printf("\n Starting add_to_cache()  \n");
				printf("\n Adding file %s to cache \n",file);
				fflush(stdout);
			    #endif  /* DEBUG */
		if( max_entries == MAXENTRIES )	{ // thread is made to wait on cache_cond which is signalled by file_cache_unpin_files() upon freeing an object
		    printf("\n Cache is full . Cant add file to cache. Try deleting some elements from cache before adding \n");
		    pthread_cond_wait(&cache_cond, &cache_mutex); 	
		}	    
		int fptr;
	    fptr = open(file, O_CREAT | O_EXCL | O_RDWR,0777); // Try to open the file exclusively to test whether file already exist in local file system or not.
	  
		if(fptr != -1){  // File doesnt exist in local file system. 
			#ifdef DEBUG
			    printf("\n fptr is %d \n",fptr);
				fflush(stdout);
			#endif  /* DEBUG */
		    printf("\n File %s doesn't exist in local file_system. Creating new  \n",file);
		    if(ftruncate(fptr,10240) == -1) 
				perror("truncate error");
		
			}
		else { // If file already exist , open file in read only mode.
		   printf("\n File %s already exist in local file system ",file);
		   fptr = open(file, O_RDONLY);
		}
		
		file_cache* fcache; // pointer to newly added cache structure 
	    fcache = (file_cache*)malloc(sizeof(file_cache)); // Allocate memory for new file_cache object.
		fcache->next = NULL;      // Mark next pointer to NULL
		fcache->prev = NULL;      // mark prev pointer to NULL
		
		/*If cache empty , initialize head & tail with fcache */
		if( max_entries == 0 ) { // file_cache list is empty. Make the newly created file_cache object as head and tail.
			head = fcache;
			tail = fcache;
		}
		else {             // if cache not empty and not full, add fcache to end of list adn adjust pointers accordingly.
			tail->next = fcache;
			fcache->prev = tail;
			tail = fcache;
		}
		
		fcache->buf = (char*)malloc(MAXSIZE); // Allocate memory for buffer to hold data
		strcpy(fcache->filename,file);        // Copy file name to file_cache object
		if(read(fptr,fcache->buf,MAXSIZE) == -1) // Read MAXSIZE bytes to buf 
		     perror("Read error");
		#ifdef DEBUG
		        printf("\n Name of newly added file is %s",fcache->filename);
				printf("\n Contents of newly added file %s \n",fcache->buf);
				fflush(stdout);
		#endif  /* DEBUG */
		fcache->pin   = 1;  // Mark the object as pinned in cache.
		fcache->dirty = 0;  // Set dirty bit to 0 as cache and local file system are consistent at this point.
		max_entries++;      // Increment cache entry count
		
		printf("\n Cache count is : %d \n",max_entries); // Display the cache count
		close(fptr); 
		pthread_mutex_unlock(&cache_mutex);
		return fcache;
}


// Pins the given files in array 'files' with size 'num_files' in the cache.
// If any of these files are not already cached, they are first read from the
// local filesystem. If the cache is full, then some existing cache entries may
// be evicted. If no entries can be evicted (e.g., if they are all pinned, or
// dirty), then this method will block until a suitable number of cache
// entries becomes available. It is OK for more than one thread to pin the
// same file, however the file should not become unpinned until both pins
// have been removed.
//
// Is is the application's responsibility to ensure that the files may
// eventually be pinned. For example, if 'max_cache_entries' is 5, an
// irresponsible client may try to pin 4 files, and then an additional 2
// files without unpinning any, resulting in the client deadlocking. The
// implementation *does not* have to handle this.
//
// If you are not comfortable with multi-threaded programming or
// synchronization, this function may be modified to return a boolean if
// the requested files cannot be pinned due to the cache being full. However,
// note that entries in 'files' may already be pinned and therefore even a
// full cache may add additional pins to files.


/* 
 * function name : file_cache_pin_files
 * arguments     : file_cache *cache   :  Pointer to head of the file_cache list.
                   const char **files  :  Pointer to list of files to be added to cache.
                   int num_files       :  Number of files to be added to cache or No of files pointed by const char **files  
 * purpose       : To pin a list of files to cache.
 * return value  : nothing
 *
*/

void file_cache_pin_files(file_cache *cache,const char **files,int num_files) {
	int i;
	printf("\n Starting file_cache_pin_files \n");
	
	for(i=0;i<num_files;i++) {  // Search if the file exist in cache 
	    printf("\n File currently searched is %s \n",files[i]);
        file_cache* search_val = search_cache(cache,files[i]);
        pthread_mutex_lock(&cache_mutex);
	    if(search_val && search_val->pin ){ 
			printf("\n File %s already exist in cache \n",files[i]); //mutex
			search_val->pin++;
		}
	    else {
			#ifdef DEBUG
			   printf("\n File %s not found in cache. Adding it to cache. \n",files[i]);
			   fflush(stdout);
			#endif  /* DEBUG */
			search_val = add_to_cache(files[i]);
			if(search_val){
    			printf("\n File %s successfully added to cache \n",files[i]);
    		}
		}
		 pthread_mutex_unlock(&cache_mutex);
		cache = head;
	}
}

/* 
 * function name : write_to_file
 * arguments     : file_cache *cache   :  Pointer to file_cache object. This object has filename to which data is to be written. 
 * purpose       : To write data persistently to the file.
 * return value  : nothing
 *
*/

void write_to_file(file_cache *fcache){
	int rc = open(fcache->filename,O_RDWR| O_APPEND);
	if(rc != -1){
		printf("\n Open %s success \n",fcache->filename);
		if( write(rc,fcache->buf,sizeof(file_content)) != sizeof(file_content)){
			perror("Write error"); 
			return;
		}
		printf("\n Write to %s success \n",fcache->filename);
		fdatasync(rc); // to make sure all data are written to disk
	}
   else {
	 perror("Open error");
	 return;
   }
   close(rc);
   fcache->dirty =0;  // Make the dirty flag 0 to indicate that data is written to disk
   strcpy(fcache->buf,""); // Make the buffer of file_cache object empty
}

// Unpin one or more files that were previously pinned. It is ok to unpin
// only a subset of the files that were previously pinned using
// file_cache_pin_files(). It is undefined behavior to unpin a file that wasn't
// pinned.

/* 
 * function name : file_cache_unpin_files
 * arguments     : file_cache *cache : Pointer to head of file_cache object.
				   char **files      : List of files to be unpinned
				   num_files         : Number of files in the list
 * purpose       : To unpin files from cache.
 * return value  : nothing
 *
*/

void file_cache_unpin_files(file_cache *cache,const char **files,int num_files){ 
	int i;
	for(i=0;i<num_files;i++){  // Search for the file in the file_cache list
		file_cache *search_val = search_cache(cache,files[i]);
		    pthread_mutex_lock(&cache_mutex); // Lock mutex before freeing up cache object
			if(search_val){
				if(search_val->dirty == 1){ // If its a dirty object , write contents to disk before unpinning.
				    #ifdef DEBUG
				     printf("\n cache dirty. Write contents to file before unpininng \n");
				     fflush(stdout);
			         #endif  /* DEBUG */
					write_to_file(search_val);
				}
				search_val->pin--;
				if(search_val->pin == 0 ){ // Remove only when pin becomes 0 , that is no client is accessing file
					// Code to adjust pointers before freeing the object
					if(search_val == head){ 
						head = search_val->next;
					}
					else if(search_val == tail){
						tail = search_val->prev;
					}
					else {
						search_val->prev->next = search_val->next;
						search_val->next->prev = search_val->prev;
					}
					free(search_val);    // free file_cache object
					max_entries--;      // reduce max_entries to reflect total no of objects 
					#ifdef DEBUG
					  printf("\n Decreasing max_cache_entries. new value is %d \n",max_entries);
					  fflush(stdout);
					#endif  /* DEBUG */
					 printf("\n File %s successfully unpinned \n",files[i]);
					 pthread_mutex_unlock(&cache_mutex);
                     pthread_cond_signal(&cache_cond); // Signal to indicate that cache can accomodate one more element
				}
				cache = head;
				
			}
			else {
				printf("\n File %s not found in cache \n",files[i]);	
			}
	 }
}


// Provide read-only access to a pinned file's data in the cache.
//
// It is undefined behavior if the file is not pinned, or to access the buffer
// when the file isn't pinned.

/* 
 * function name : file_cache_file_data
 * arguments     : file_cache *cache   : Pointer to the head of the file_cache list
                   const char *file    : Name of the file from which data to be read.
 * purpose       : to read data from cache . If file not in cache , add file to cache and then read data.
 * return value  : pointer to the buffer containing data read.
 *
*/

const char *file_cache_file_data(file_cache *cache, const char *file){
	
	file_cache *search_val = search_cache(cache,file); // Check if file exist in cache.
		if(search_val && search_val->pin){
			#ifdef DEBUG
				printf("\n File %s exist in cache \n",search_val->filename);
				fflush(stdout);
			#endif  /* DEBUG */
			return search_val->buf;         // If exist read from cache.
		}
		else {
			#ifdef DEBUG
				printf("\n File not found in cache. Adding it to cache. \n");
				fflush(stdout);
			#endif  /* DEBUG */
			search_val = add_to_cache(file); // File not found in cache . Add file to cache and read.
			return search_val->buf;
		}
	
}


// Provide write access to a pinned file's data in the cache. This call marks
// the file's data as 'dirty'. The caller may update the contents of the file
// by writing to the memory pointed by the returned value.
//
// Multiple clients may have access to the data, however the cache *does not*
// have to worry about synchronizing the clients' accesses (you may assume
// the application does this correctly).
//
// It is undefined behavior if the file is not pinned, or to access the buffer
// when the file is not pinned.

/* 
 * function name : file_cache_mutable_file_data
 * arguments     : file_cache *cache   : Pointer to the head of the file_cache list
                   const char *file    : Name of the file to which data is to be written.
 * purpose       : To write data to cache . If file not in cache , add file to cache and then write data. 
                   Note : this function keeps data and in the cache only and will not write it to local filesystem.
 * return value  : pointer to the buffer containing data written.
 *
*/

char *file_cache_mutable_file_data(file_cache *cache, const char *file){
	printf("\n File to be changed is %s",file);
	file_cache *search_val = search_cache(cache,file);
	
	// If file exist in cache and already dirty , flush contents to storage before writing new content.
	if(search_val && search_val->pin && search_val->dirty){
	       write_to_file(search_val);
	}	
	
	// write to cache when entry exist in cache and is pinned and is not dirty already 
	else if(search_val && search_val->pin && !search_val->dirty){
	        printf("\n File %s exist in cache.\n",file);
			printf("\n Copy file_content to buffer \n");
		    strcpy(search_val->buf,file_content);
	}
	// File doesnt exist in cache. Add it to cache and update buffer of file_cache object.
	else {
	       #ifdef DEBUG
			  printf("\n File not found in cache. Adding it to cache. \n");
			  fflush(stdout);
			#endif  /* DEBUG */
			search_val = add_to_cache(file);
			strcpy(search_val->buf,file_content);
	}
	search_val->dirty = 1;		// make dirty bit to 1 to indicate that buffer has some data which is not yet written to disk.
	return search_val->buf;
}



