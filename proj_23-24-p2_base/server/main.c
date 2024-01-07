#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

int server_pipe;
sigset_t blocked_signals;
int sigusr1_flag = 0;

void handle_function(){
  sigusr1_flag = 1;
  printf("SIGUSR1 received\n");
}

typedef struct{
  int client_session_id;
}thread_args;

typedef struct{
  char *req_pipe_path, *resp_pipe_path;
} Client;

Client clients[MAX_SESSION_COUNT];
int client_count = 0;
pthread_mutex_t clients_mutex;
pthread_cond_t clients_cond;

void*thread_function(void* args){

  thread_args *t_args = (thread_args*)args;
  int active_client;
  int client_session_id = t_args->client_session_id;

  // Blocks thread from receiving
  pthread_sigmask(SIG_BLOCK, &blocked_signals, NULL);

  // Loop that keeps thread always active
  while(1) {

    pthread_mutex_lock(&clients_mutex);

    while(client_count == 0 ){
      printf("Consumer %d is waiting...\n", client_session_id);
      pthread_cond_wait(&clients_cond, &clients_mutex); // Unlocks mutex and waits for conditional signal
    }

    // If we get a conditional signal and the client count is greater than 0 that means we have a new active client
    active_client = 1;
    char* req_pipe_path = clients[client_count-1].req_pipe_path;
    char* resp_pipe_path = clients[client_count-1].resp_pipe_path;

    client_count--;
    pthread_mutex_unlock(&clients_mutex);
    printf("Consumer %d is awake.\n", client_session_id);

    int req_pipe = open(req_pipe_path, O_RDONLY);
    if (req_pipe == -1) {
      perror("Error opening Client's request pipe for reading");
    }

    // Opens Client's response pipe for writing
    int resp_pipe = open(resp_pipe_path, O_WRONLY);
    if (resp_pipe == -1) {
      perror("Error opening Client's response pipe for writing");
      close(req_pipe);
    }

    // Sends the session_id back to Client via resp_pipe
    if (write(resp_pipe, &client_session_id, sizeof(int)) == -1) {
      fprintf(stderr, "Failed to write session_id to response pipe\n");
    }

    printf("\n");

    printf("A Client connected to the server with session ID: %d!\n", client_session_id);

    while (1) {


      if (!active_client) {
        break;
      }

      char op_code;
      ssize_t bytes_read;
      bytes_read = read(req_pipe, &op_code, sizeof(char));

      if (bytes_read == -1) {
        // fprintf(stderr, "Error reading OP_CODE from request pipe\n");
        perror("Error reading OP_CODE from request pipe");
        close(req_pipe);
        close(resp_pipe);
      }

      // Switch case to execute commands based on OP_CODE
      switch (op_code) {
        case '2': {
          int session_id;

          if (read(req_pipe, &session_id, sizeof(int)) == -1) {
            fprintf(stderr, "Error reading from request pipe (ems_quit)\n");
          }

          printf("REQUEST FOR EMS_QUIT RECEIVED\n");

          close(req_pipe);
          close(resp_pipe);

          active_client = 0;

          printf("Client with session ID %d disconnected from server!\n", session_id);

          break;
        }
        case '3': {
          int session_id;
          unsigned int event_id;
          size_t num_rows;
          size_t num_cols;

          if (read(req_pipe, &session_id, sizeof(int)) == -1 || read(req_pipe, &event_id, sizeof(unsigned int)) == -1 ||
              read(req_pipe, &num_rows, sizeof(size_t)) == -1 || read(req_pipe, &num_cols, sizeof(size_t)) == -1) {
            fprintf(stderr, "Error reading from request pipe (ems_create)\n");
          }

          printf("REQUEST FOR EMS_CREATE RECEIVED\n");

          int return_status = ems_create(event_id, num_rows, num_cols);

          if (write(resp_pipe, &return_status, sizeof(int)) == -1) {
            fprintf(stderr, "Error writing return status to response pipe (ems_create)\n");
          }

          break;
        }

        case '4': {
          int session_id;
          unsigned int event_id;
          size_t num_seats;

          if (read(req_pipe, &session_id, sizeof(int)) == -1 || read(req_pipe, &event_id, sizeof(unsigned int)) == -1 ||
              read(req_pipe, &num_seats, sizeof(size_t)) == -1) {
            fprintf(stderr, "Error reading from request pipe (ems_reserve)\n");
          }

          size_t xs[num_seats];
          size_t ys[num_seats];

          if (read(req_pipe, &xs, sizeof(size_t) * num_seats) == -1 ||
              read(req_pipe, &ys, sizeof(size_t) * num_seats) == -1) {
            fprintf(stderr, "Error reading reservation seat coordinates from request pipe (ems_reserve)\n");
          }

          printf("REQUEST FOR EMS_RESERVE RECEIVED\n");

          int return_status = ems_reserve(event_id, num_seats, xs, ys);

          if (write(resp_pipe, &return_status, sizeof(int)) == -1) {
            fprintf(stderr, "Error writing return status to response pipe (ems_reserve)\n");
          }

          break;
        }

        case '5': {
          int session_id;
          unsigned int event_id;

          if (read(req_pipe, &session_id, sizeof(int)) == -1 || read(req_pipe, &event_id, sizeof(unsigned int)) == -1) {
            fprintf(stderr, "Error reading from request pipe (ems_show)\n");
          }

          printf("REQUEST FOR EMS_SHOW RECEIVED\n");
          ems_show(resp_pipe, event_id);

          break;
        }

        case '6': {
          int session_id;

          if (read(req_pipe, &session_id, sizeof(int)) == -1) {
            fprintf(stderr, "Error when reading session_id from request pipe (ems_list_events)\n");
          }

          printf("REQUEST FOR EMS_LIST_EVENTS RECEIVED\n");
          ems_list_events(resp_pipe);
          break;
        }

        default: {
          break;
        }

      }
    }
  }
}



int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' ||  delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  sigemptyset(&blocked_signals);
  signal(SIGUSR1, handle_function);
  sigaddset(&blocked_signals, SIGUSR1);

  //Initializes clients mutex and condition variable
  pthread_mutex_init(&clients_mutex, NULL);
  pthread_cond_init(&clients_cond, NULL);

  pthread_t thread_array[MAX_SESSION_COUNT];
  thread_args  args_array[MAX_SESSION_COUNT];

  for(int i=0; i<MAX_SESSION_COUNT; i++){
    // Assigns session id to thread id
    args_array[i].client_session_id = i+1;
    pthread_create(&thread_array[i], NULL, thread_function, &args_array[i]);
  }

  // Creates server pipe with name from command line
  if (mkfifo(argv[1], 0777) == -1){
    if (errno != EEXIST){
      fprintf(stderr, "Failed to create server pipe\n");
      return 1;
    }
  }

  // Opens server pipe for reading
  if ((server_pipe = open(argv[1], O_RDONLY) )== -1){
    fprintf(stderr, "Failed to open server pipe\n");
    return 1;
  }

  printf("Server is now running...\n");
  printf("\n");

  // Registration while loop
  while(1){
    char OP_CODE = '0';
    char req_pipe_path[MAX_PIPE_NAME];
    char resp_pipe_path[MAX_PIPE_NAME];
    Client client;
    ssize_t bytes_read;

    if(sigusr1_flag){
      sigusr1_flag = 0;
      ems_print_info(STDOUT_FILENO);
    }

    // Reads from server pipe to initialize a new session
    if ((bytes_read = read(server_pipe, &OP_CODE, sizeof(char))) == -1){
      fprintf(stderr, "Failed to read from request pipe\n");
      return 1;
    } else if(bytes_read == 0){
      continue;
    }

    if(OP_CODE != '1'){
      continue;
    }

    if (read(server_pipe, req_pipe_path, MAX_PIPE_NAME) == -1){
      fprintf(stderr, "Failed to read from request pipe\n");
      return 1;
    }

    if (read(server_pipe, resp_pipe_path, MAX_PIPE_NAME) == -1){
      fprintf(stderr, "Failed to read from request pipe\n");
      return 1;
    }

    client.req_pipe_path = req_pipe_path;
    client.resp_pipe_path = resp_pipe_path;

    pthread_mutex_lock(&clients_mutex);

    // Adds new client to the array of clients
    clients[client_count] = client;
    client_count++;

    // Signals the threads that are waiting to acquire a client and execute its requests
    pthread_cond_signal(&clients_cond);
    pthread_mutex_unlock(&clients_mutex);

  }

}