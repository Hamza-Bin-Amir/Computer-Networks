/* A simple file transfer server using TCP */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <strings.h>

#define SERVER_TCP_PORT 3000   /* well-known port */
#define BUFLEN 256             /* buffer length */
#define CHUNK_SIZE 100         // Define the chunk size

int file_transfer(int);
void reaper(int);

int main(int argc, char **argv)
{
    int sd, new_sd, client_len, port; // Declare socket and client variables.
    struct sockaddr_in server, client; // Declare server and client address structures.

    switch (argc)
    {
    case 1:
        port = SERVER_TCP_PORT; // Default port if not provided as a command-line argument.
        break;
    case 2:
        port = atoi(argv[1]); // Port specified as a command-line argument.
        break;
    default:
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(1);
    }

    /* Create a stream socket    */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Can't create a socket\n");
        exit(1);
    }

    /* Bind an address to the socket    */
    bzero((char *)&server, sizeof(struct sockaddr_in)); // Initialize server structure with zeros.
    server.sin_family = AF_INET; // Set the address family to IPv4.
    server.sin_port = htons(port); // Set the port number in network byte order.
    server.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to any available network interface.

    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) // Bind the socket to the server address.
    {
        fprintf(stderr, "Can't bind name to socket\n");
        exit(1);
    }

    /* Queue up to 5 connect requests  */
    listen(sd, 5); // Listen for incoming client connections, with a backlog of 5.

    (void)signal(SIGCHLD, reaper); // Set up a signal handler for child process termination.

    while (1)
    {
        client_len = sizeof(client); // Get the size of the client address structure.
        new_sd = accept(sd, (struct sockaddr *)&client, &client_len); // Accept a new client connection.
        if (new_sd < 0)
        {
            fprintf(stderr, "Can't accept client\n");
            exit(1);
        }
        switch (fork())
        {
        case 0: // Child process
            close(sd); // Close the original socket in the child process.
            exit(file_transfer(new_sd)); // Execute the file_transfer function in the child.
        default: // Parent process
            close(new_sd); // Close the new socket in the parent.
            break;
        case -1:
            fprintf(stderr, "fork: error\n");
        }
    }
}

/*	file_transfer program	*/
int file_transfer(int sd)
{
    char filename[255]; // Declare a buffer to store the filename.
    char *error_msg = "file DNE\n"; // Declare an error message for file not found.
    size_t length = strlen(error_msg); // Calculate the length of the error message.

    ssize_t bytes_received = read(sd, filename, sizeof(filename)); // Read the filename from the client.
    if (bytes_received <= 0) // Check if the filename read failed.
    {
        fprintf(stderr, "Receiving filename failed\n");
        close(sd);
        exit(EXIT_FAILURE);
    }
    filename[bytes_received] = '\0'; // Null-terminate the received filename.

    FILE *file = fopen(filename, "rb"); // Open the file for reading in binary mode.
    if (file == NULL) // Check if the file could not be opened.
    {
        write(sd, error_msg, length); // Send the error message to the client.
        fprintf(stderr, "File open failed\n");
        close(sd);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END); // Move the file pointer to the end of the file to get its size.
    long file_size = ftell(file); // Get the file size in bytes.
    fseek(file, 0, SEEK_SET); // Reset the file pointer to the beginning of the file.

    char buffer[CHUNK_SIZE]; // Declare a buffer to hold data chunks.

    ssize_t bytes_sent;
    size_t total_bytes_sent = 0;

    while (total_bytes_sent < file_size) // Loop until the entire file is sent.
    {
        size_t remaining_bytes = file_size - total_bytes_sent; // Calculate the remaining bytes to send.
        size_t send_size;

        if (remaining_bytes < CHUNK_SIZE)
        {
            send_size = remaining_bytes; // Send the remaining bytes if less than CHUNK_SIZE.
        }
        else
        {
            send_size = CHUNK_SIZE; // Send CHUNK_SIZE bytes if more data is available.
        }

        // Read a chunk of data from the file into the buffer
        size_t bytes_read = fread(buffer, 1, send_size, file);
        if (bytes_read != send_size)
        {
            fprintf(stderr, "Reading file chunk failed\n");
            close(sd);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        // Send the chunk of data to the client
        ssize_t chunk_bytes_sent = write(sd, buffer, send_size);

        if (chunk_bytes_sent == -1) // Check if sending a chunk failed.
        {
            fprintf(stderr, "Sending file chunk failed\n");
            close(sd);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        total_bytes_sent += chunk_bytes_sent; // Update the total bytes sent.
    }

    printf("File successfully sent\n"); // Print a success message.

    fclose(file); // Close the file.
    close(sd); // Close the socket.

    return 0;
}

/*	reaper		*/
void reaper(int sig)
{
    int status;
    while (wait3(&status, WNOHANG, (struct rusage *)0) >= 0); // Reap child processes that have terminated.
}
