# Event Management System

## Description

This project is a multi-threaded Event Management System featuring a robust client-server architecture. It excels at efficiently handling events and solving challenges related to the producer-consumer problem. 
The Event Manager receives input via `.jobs` files located inside the `jobs` directory. After executing the commands specified in the `.jobs` file, the program creates a corresponding `.out` file in the same directory containing the output.

- Refer to the file `Projeto SO - Parte 2.pdf` for the project's instructions and details.

# Configuration

- To run the program you first must compile it using:
> **make**

- After compiling you must run the server's executable inside the `server` directory using:
> **./ems pipe_name** (where pipe_name is the name of the server's designated pipe for receiving client connection requests.)  

- With the server already running, you can now run client instances in the `client` directory using:
> **./client req_pipe resp_pipe server_pipe jobs_file_path**

Where:
- **req_pipe** is the path to the client's request pipe-
- **resp_pipe** is the path to the client's response pipe.
- **server_pipe** is the path to the server's pipe that was created upon server initialization.
- **jobs_file_path** is the file containing the commands to be executed by the program.
