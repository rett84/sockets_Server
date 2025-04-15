#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#define PORT 9000
#define MAX_SIZE 1000

volatile sig_atomic_t gSignalInterrupt = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
bool thread_completed;
int t_index;
char store_file[] =  "/var/tmp/aesdsocketdata";    


//handler for SIGINT and SIGTERM
static void signal_handler (int signo)
{
    if (signo == SIGINT)
    {
        printf ("Caught signal, exiting SIGINT!\n");
        gSignalInterrupt = 1;
    }  
    else if (signo == SIGTERM)
    {
        printf ("Caught signal, exiting SIGTERM!\n");
        gSignalInterrupt = 1;
    }
    else 
    {
        /* this should never happen */
        fprintf (stderr, "Unexpected signal!\n");
        exit (EXIT_FAILURE);
    }

}


struct thread_data{;
    int new_socket;
    char *ip_address;
    int t_index;
};

struct node {
    int t_index; //thread index identifier
    pthread_t tid;
    struct node* next;
};

//Function to Insert into head of linked list
 void Insert_to_List(struct node** headRef, int t_index) {
    struct node* newNode = malloc(sizeof(struct node));

    (*newNode).t_index = t_index;
    (*newNode).next = *headRef; // The '*' to dereferences back to the real head
    *headRef = newNode; 
} 

//Function to free linked list
void freeList(struct node* head)
{
   struct node* tmp;
   while (head != NULL)
    {
       tmp = head;
       head = head->next;
       free(tmp);
    }
}

//Socket Thread
void* threadsocket(void* thread_param)
{
    ssize_t valread =0;
    size_t len_packet = 0;
    size_t len_buf = 0;
    
    char buffer[2000];
    char new_line_char[] = "\n"; //new line variable
    
     // declaring file pointers
    FILE *fp1;

    struct thread_data* thread_func_args;
    thread_func_args = (struct thread_data *) thread_param;

    int new_socket = (*thread_func_args).new_socket;
    char * ip_address = (*thread_func_args).ip_address;

    

    while(1)
    {
        
        //char * store_file = (*thread_func_args).store_file;

        valread = recv(new_socket , buffer , sizeof(buffer),0);

        if (valread>0)
        {

            printf("Echo Message: %s",buffer);
            //get length of buffer
            len_buf = strlen(buffer)-1;

            // opening file in append mode
            fp1 = fopen(store_file, "a");

            //append packet data to file
            pthread_mutex_lock(&lock);
            fwrite(buffer, sizeof(char), strlen(buffer), fp1); 
            pthread_mutex_unlock(&lock);
            //close file
            fclose(fp1);


          //end of packet compare
           if (buffer[len_buf] == new_line_char[0])
            {

                char *line = NULL;
                size_t len = 0;
               
                // opening file in read mode
                fp1 = fopen(store_file, "r");

                
                //read line-by-line and send it to client
                 while(getline(&line, &len, fp1) != -1) 
                {
                   // printf("Sending back: %s", line);

                    //send data back to client
                    send(new_socket, line, strlen(line), 0);
                } 

                fclose(fp1);
                free(line);
                memset(buffer, '\0', sizeof(buffer));
            }
        }
   
        if (valread<=0) // client closed connection, buffer empty
        {          
            printf("Closed connection from  %s\n", ip_address);
            syslog(LOG_DEBUG,"Closed connection from  %s\n", ip_address);
            if (!thread_completed)
            {
                pthread_mutex_lock(&lock);
                t_index = (*thread_func_args).t_index;
                thread_completed =true;
                free(thread_param);
                pthread_mutex_unlock(&lock);
            }
            
            //printf("Exit socketThread FD %i \n",new_socket);
            close(new_socket);
            break;

        }
    }  


    pthread_exit(NULL); 
    return thread_param;     
}

bool write_timestamp(struct timeval t1, struct timeval t2)
{
    char str[255];
    char rfc_2822[40];

   
    // declaring file pointers
    FILE *fp1;

    time_t rawtime;
    struct tm * timeinfo;
    //struct timeval t1, t2;
    double elapsedTime;
   

        // compute and print the elapsed time
        elapsedTime = (t2.tv_sec - t1.tv_sec);// sec 

        //sleep(10);
       
        if (elapsedTime>=10)
        {
            time ( &rawtime );
            timeinfo = localtime ( &rawtime );
            
            strftime(rfc_2822,sizeof(rfc_2822),"%a, %d %b %Y %T %z",timeinfo);
            printf("%s\n", rfc_2822);
            strcpy(str,"timestamp:");
            strcat(str, rfc_2822);
            strcat(str, "\n");

            // opening file in append mode   
            fp1 = fopen(store_file, "a");
            //append packet data to file
            pthread_mutex_lock(&lock);
            fwrite(str, sizeof(char), strlen(str), fp1); 
            pthread_mutex_unlock(&lock);
            //close file
            fclose(fp1);
            
            return true;
            exit;
           
        
        } 
        return false;
}



int main(int argc, char *argv[])
{
    /*VARIABLES DECLARATIONS*********************
    ********************************
    ********************************
    */
    int server_fd=0;
    int new_socket=0;
    //char store_file[] =  "/var/tmp/aesdsocketdata";
    char ip_address[INET_ADDRSTRLEN];
 
    
    struct sockaddr_in address;
    int opt = 1;
   
    
  
    char *daemon_mode = argv[1]; //daemon parameter
    char *par = "-d";
   

    struct node* head = NULL;//
    int i = 0;

    //sigaction struct
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;

    //Register signal handlers
    //signal(SIGTERM, signal_handler);
    //signal(SIGINT, signal_handler);
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
    
    //removes aesdsocketdata file
    remove(store_file);

    //Open Log file
    //openlog(NULL, 0, LOG_USER);

    if (pthread_mutex_init(&lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return 1; 
    } 

    //TimeStamp Thread
   // pthread_t tid_ts;

   // pthread_create(&tid_ts, NULL, thread_timestamp, 0);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the PORT
    // Prevents error such as: “address already in use”.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //parameters of TCP connection
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);


    // Forcefully attaching socket to the PORT
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address))< 0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //check if -d argument was passed
    if (daemon_mode != NULL)
    {
        if (strcmp(daemon_mode,par)  == 0)
        {
            pid_t process_id = 0;
            // Create child process
            process_id = fork();
            // Indication of fork() failure
            if (process_id < 0)
                {
                    printf("Can't create the child proccess");
                    // Return failure in exit status
                    exit(EXIT_FAILURE);
                }


            // killing the parent process (child process will be an orphan)
            if (process_id > 0)
            {
                //printf("process_id of child process %d \n", process_id);
                // return success in exit status
                exit(EXIT_SUCCESS);
            }
        }
    }
    


    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Waiting for connection...\n");
    }


    struct timeval timeout;
    timeout.tv_sec = 0.5;  // timeout for select
    timeout.tv_usec = 0;


    struct timeval t1, t2;
    gettimeofday(&t1, NULL);// get time
    //loop while waiting for client to connect
    while (gSignalInterrupt !=1) //)gSignalInterrupt !=1
    {

      
        //Compute elapsed time
        gettimeofday(&t2, NULL);
        bool ret;
        ret = write_timestamp(t1,t2);
        if (ret)
        {
            gettimeofday(&t1, NULL);// get time
        }

       
       
           

            fd_set read_fds;
            int fdmax=0;
            int select_status = 0;
           // 

            FD_ZERO(&read_fds);
            fdmax = server_fd;
            FD_SET(server_fd,&read_fds);

            if (server_fd >= fdmax) 
            {
                fdmax = server_fd + 1;
            }

            select_status = select(fdmax, &read_fds, NULL, NULL, &timeout);
            if (select_status == -1) 
            {
                //perror("listen");
                //exit(EXIT_FAILURE);

                gSignalInterrupt = 1;
                break;
            } 
            else if (select_status > 0) 
            {
               
                // Accept a connection
                socklen_t addrlen = sizeof(address);
                new_socket = accept(server_fd, (struct sockaddr*)&address,&addrlen);
                if ((new_socket) < 0) 
                {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                printf("Accepted connection from  %s\n", inet_ntoa(address.sin_addr));
                syslog(LOG_DEBUG,"Accepted connection from  %s\n", inet_ntoa(address.sin_addr));

                //Struct for thread data
                struct thread_data *args = malloc(sizeof *args); //malloc here fixed use of repeated socket FD
                //args = realloc(args,sizeof *args);
                //convert IP address to char
                inet_ntop(AF_INET, &(address.sin_addr), ip_address, INET_ADDRSTRLEN);
                i++;
                //Parameters for Thread Socket function
                (*args).new_socket = new_socket;
               // (*args).store_file = store_file;
                (*args).ip_address = ip_address;
                (*args).t_index = i;

                
                //Add item to head of linked list when there is a new connection
                 Insert_to_List(&head, i);
                
                
                //for each client request creates a thread and assign the client request to it to process
                //so the main thread can entertain next request
                if( pthread_create(&head->tid, NULL, threadsocket, args) != 0 )
                    printf("Failed to create thread\n");

                
            }

            //Check which thraad has completed and join it
            if (thread_completed)
            {
                struct node* current = NULL;//
                for(current = head; current != NULL; current = current->next)
                {                   
                    if((*current).t_index == t_index)
                    {
                        pthread_join((*current).tid,NULL);
                        thread_completed = false;
                        t_index = 0;
                    }
                } 
            }  
        
    }

    printf("Exiting application\n");
    //removes aesdsocketdata file
    remove(store_file);
    // closing the listening socket
    close(server_fd);
    //release memory
    //free(args);
    freeList(head);
    return 0;
    exit (EXIT_SUCCESS);
}
