#include "api.h"

int req_pipe, resp_pipe, server_pipe;
const char *pipe1_path, *pipe2_path;
int session_id;


/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(size_t num_cols, size_t row, size_t col) { return (row - 1) * num_cols + col - 1; }

int ems_setup(char const *req_pipe_path, char const *resp_pipe_path, char const *server_pipe_path) {

  pipe1_path = req_pipe_path;
  pipe2_path = resp_pipe_path;
  unlink(req_pipe_path);
  unlink(resp_pipe_path);

  // Creates the request pipe
  if (mkfifo(req_pipe_path, 0777) == -1) {
    fprintf(stderr, "Error creating request pipe\n");
    ems_quit();
    return 1;
  }

  // Creates the response pipe
  if (mkfifo(resp_pipe_path, 0777) == -1) {
    fprintf(stderr, "Error creating response pipe\n");
    ems_quit();
    return 1;
  }

  // Opens server pipe
  if((server_pipe = open(server_pipe_path, O_WRONLY)) == -1) {
    fprintf(stderr, "Error opening server pipe\n");
    ems_quit();
    return 1;
  }

  // Builds request message to initiate session: THIS IS NEVER REACHED
  char op_code = '1';
  char buffer[81];
  memset(buffer, '\0', sizeof(char)*81);
  memcpy(buffer, &op_code, sizeof(char));
  memcpy(buffer + sizeof(char), req_pipe_path, sizeof(char)*40);
  memcpy(buffer + sizeof(char) + sizeof(char)*40, resp_pipe_path, sizeof(char)*40);


  // Connects to the server by sending request message
  if (write(server_pipe, buffer, 81) == -1) {
    fprintf(stderr, "Error sending request to the server\n");
    close(server_pipe);
    ems_quit();
    return 1;
  }

  printf("Connecting to server...\n");
  printf("\n");

  // Opens the request pipe for writing
  if ((req_pipe = open(req_pipe_path, O_WRONLY)) == -1) {
    fprintf(stderr, "Error opening request pipe.\n");
    close(server_pipe);
    ems_quit();
    return 1;
  }


  // Open the response pipe for reading
  if ((resp_pipe = open(resp_pipe_path, O_RDONLY)) == -1) {
    fprintf(stderr, "Error opening response pipe.\n");
    close(server_pipe);
    ems_quit();
    return 1;
  }

  // Reads the session_id from the response pipe
  if (read(resp_pipe, &session_id, sizeof(int)) == -1) {
    fprintf(stderr, "Error reading session_id from the response pipe\n");
    close(server_pipe);
    ems_quit();
    return 1;
  }

  printf("Connection established with session ID = %d.\n", session_id);
  printf("\n");

  // Cleanup: Close pipes
  close(server_pipe);

  return 0;  // Success
}


// Terminates session
int ems_quit(void) {
  //TODO: close pipes

  char op_code = '2';

  // Sends request to terminate session
  if(write(req_pipe, &op_code, sizeof(char)) == -1 || (write(req_pipe, &session_id, sizeof(int)) == -1)){
    fprintf(stderr, "Error writing to request pipe (ems_quit)\n");
    close(req_pipe);
    close(resp_pipe);
    close(server_pipe);
    unlink(pipe1_path);
    unlink(pipe2_path);
    return 1;
  }

  printf("REQUEST FOR EMS_QUIT SENT!\n");

  close(req_pipe);
  close(resp_pipe);
  close(server_pipe);
  unlink(pipe1_path);
  unlink(pipe2_path);

  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  char op_code = '3';

  //Sends requests

  if (write(req_pipe, &op_code, sizeof(char)) == -1 ||
      write(req_pipe, &session_id, sizeof(int)) == -1 ||
      write(req_pipe, &event_id, sizeof(unsigned int)) == -1 ||
      write(req_pipe, &num_rows, sizeof(size_t)) == -1 ||
      write(req_pipe, &num_cols, sizeof(size_t)) == -1) {
    fprintf(stderr, "Error writing to request pipe (ems_create)\n");
    ems_quit();
    return 1;
  }

  printf("REQUEST FOR EMS_CREATE SENT!\n");

  // Receives response
  int return_status;
  if(read(resp_pipe, &return_status, sizeof(int)) == -1){
    fprintf(stderr, "Error reading from response pipe (ems_create)\n");
    ems_quit();
    return 1;
  }

  if(return_status == 1){
    fprintf(stderr, "EMS_CREATE FAILED (ems_show)\n");
    return 1;
  }
  // Returns 0 on success
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  char op_code = '4';

  // Sends requests
  if (write(req_pipe, &op_code, sizeof(char)) == -1 ||
      write(req_pipe, &session_id, sizeof(int)) == -1 ||
      write(req_pipe, &event_id, sizeof(unsigned int)) == -1 ||
      write(req_pipe, &num_seats, sizeof(size_t)) == -1 ||
      write(req_pipe, xs, sizeof(size_t) * num_seats) == -1 ||
      write(req_pipe, ys, sizeof(size_t) * num_seats) == -1) {
    fprintf(stderr, "Error writing to request pipe (ems_reserve)\n");
    ems_quit();
    return 1;
  }

  printf("REQUEST FOR EMS_RESERVE SENT!\n");

  // Receives response
  int return_status;
  if (read(resp_pipe, &return_status, sizeof(int)) == -1) {
    fprintf(stderr, "Error reading from response pipe (ems_reserve)\n");
    ems_quit();
    return 1;
  }

  if(return_status == 1){
    fprintf(stderr, "EMS_RESERVE FAILED (ems_reserve)\n");
    return 1;
  }

  // Returns 0 on success
  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {

  char op_code = '5';

  // Sends request
  if (write(req_pipe, &op_code, sizeof(char)) == -1 ||
      write(req_pipe, &session_id, sizeof(int)) == -1 ||
      write(req_pipe, &event_id, sizeof(unsigned int)) == -1) {
    fprintf(stderr, "Error writing to request pipe (ems_show)\n");
    ems_quit();
    return 1;
  }

  printf("REQUEST FOR EMS_SHOW SENT!\n");

  int ret_value;
  size_t num_rows;
  size_t num_cols;


  // Reads return value from response pipe
  if( read(resp_pipe, &ret_value, sizeof(int)) == -1 ){
    fprintf(stderr, "Error reading return value from request pipe (ems_show)\n");
    ems_quit();
    return 1;
   }

   if(ret_value == 1){
      fprintf(stderr, "EMS_SHOW FAILED (ems_show)\n");
      ems_quit();
      return 1;
   } else {

      // Reads num_rows and num_cols from response pipe
      if(read(resp_pipe, &num_rows , sizeof(size_t)) == -1 || read(resp_pipe, &num_cols , sizeof(size_t)) == -1) {
        fprintf(stderr, "Error reading num_rows or num_cols from request pipe (ems_show)\n");
        ems_quit();
        return 1;
      }

      unsigned int seats[num_rows*num_cols];
      // Reads room layout from response pipe
      if(read(resp_pipe, &seats, sizeof(unsigned int)* num_rows * num_cols) == -1){
        fprintf(stderr, "Error reading seats layout from request pipe (ems_show)\n");
        ems_quit();
        return 1;
      }

      // Prints the layout of the room to the output file
      for (size_t i = 1; i <= num_rows; i++) {
        for (size_t j = 1; j <= num_cols; j++) {

          char seat_str[2];
          sprintf(seat_str, "%u", seats[seat_index(num_cols, i, j)]);

          if (write(out_fd, seat_str, 1) == -1) {
            fprintf(stderr, "Error writing seat to output file (ems_show)\n");
            ems_quit();
            return 1;
          }

          if(j < num_cols){
            if (write(out_fd, " ", sizeof(char)) == -1) {
              fprintf(stderr, "Error writing space in seat layout to output file (ems_show)\n");
              ems_quit();
              return 1;
            }
          }
        }

        if (write(out_fd, "\n", sizeof(char)) == -1) {
          fprintf(stderr, "Error writing newline in seat layout to output file (ems_show)\n");
          ems_quit();
          return 1;
        }
     }
   }

   // Returns 0 on success
  return 0;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)

  char op_code = '6';

  // Sends request
  if((write(req_pipe, &op_code, sizeof(char)) == -1) || write(req_pipe, &session_id, sizeof(int)) == -1){
      ems_quit();
      return 1;
  }

  printf("REQUEST FOR EMS_LIST_EVENTS SENT!\n");

  int ret_value;
  // Reads return value from response pipe
  if( read(resp_pipe, &ret_value, sizeof(int)) == -1 ){
      fprintf(stderr, "Error reading return value from request pipe (ems_show)\n");
      ems_quit();
      return 1;
  }

  if(ret_value == 1){
      fprintf(stderr, "EMS_LIST_EVENTS FAILED\n");
      return 1;

  } else if(ret_value == 2){

      char no_events[] = "No Events\n";

      if(write(out_fd, &no_events, sizeof(no_events)) == -1){
        fprintf(stderr,"Error writing 'No Events' to out file descriptor\n");
        return 1;
      }

      return 2;

  } else{

      size_t num_events;

      if( read(resp_pipe, &num_events, sizeof(size_t)) == -1 ){
        fprintf(stderr, "Error reading num_events from request pipe (ems_list_events)\n");
        ems_quit();
        return 1;
      }

      unsigned int ids[num_events];

      if( read(resp_pipe, &ids, sizeof(unsigned int)*num_events) == -1 ){
        fprintf(stderr, "Error reading num_events from request pipe (ems_list_events)\n");
        ems_quit();
        return 1;
      }

      for(size_t i = 0; i < num_events; i++ ){

        char buff[] = "Event: ";

        if (write(out_fd, &buff, strlen(buff)) == -1) {
          fprintf(stderr,"Error writing 'No Events' to out file descriptor\n");
          return 1;
        }

        char event_id_str[2];
        sprintf(event_id_str, "%u", ids[i]);

        if(write(out_fd, event_id_str, 1) == -1) {
          fprintf(stderr, "Error writing event ID to out file descriptor\n");
          return 1;
        }

        char new_line[] = "\n";
        if(write(out_fd, new_line, 1) == -1) {
          fprintf(stderr, "Error writing event ID to out file descriptor\n");
          return 1;
        }

      }

      return 0;
  }
}
