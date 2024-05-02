/* A simple file transfer client using TCP */
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>

#define SERVER_TCP_PORT 3000    /* well-known port */
#define BUFLEN      256    /* buffer length */
#define CHUNK_SIZE 100  // Define the chunk size

int main(int argc, char **argv)
{
    int n, i, bytes_to_read;
    int sd, port;
    struct hostent *hp;
    struct sockaddr_in server;
    char *host, *bp, rbuf[BUFLEN], sbuf[BUFLEN];

    switch (argc)
    {
    case 2:
        host = argv[1];
        port = SERVER_TCP_PORT;
        break;
    case 3:
        host = argv[1];
        port = atoi(argv[2]);
        break;
    default:
        fprintf(stderr, "Usage: %s host [port]\n", argv[0]);
        exit(1);
    }

    /* Create a stream socket    */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Can't create a socket\n");
        exit(1);
    }

    bzero((char *)&server, sizeof(struct sockaddr_in)); // Initialize server structure with zeros.
    server.sin_family = AF_INET; // Set the address family to IPv4.
    server.sin_port = htons(port); // Set the port number in network byte order.
    if ((hp = gethostbyname(host)))
        bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
    else if (inet_aton(host, (struct in_addr *)&server.sin_addr))
    {
        fprintf(stderr, "Can't get server's address\n");
        exit(1);
    }

    /* Connecting to the server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1) // Connect to the server.
    {
        fprintf(stderr, "Can't connect to the server\n");
        exit(1);
    }

    char filename[256]; // Define a buffer to store the filename.
    printf("Enter the filename: ");
    scanf("%255s", filename); // Read the filename from the user, limiting input to 255 characters.

    ssize_t bytes_sent = write(sd, filename, strlen(filename)); // Send the filename to the server.

    if (bytes_sent == -1) // Check if sending the filename failed.
    {
        fprintf(stderr, "Sending filename failed\n");
        close(sd);
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(filename, "wb"); // Create a new file based on the received filename.
    if (file == NULL) // Check if the file could not be opened.
    {
        fprintf(stderr, "File open failed\n");
        close(sd);
        exit(EXIT_FAILURE);
    }

    char buffer[CHUNK_SIZE]; // Declare a buffer to receive data in chunks.

    ssize_t bytes_received;
    size_t total_bytes_received = 0;

    while (1) // Infinite loop to receive data chunks.
    {
        // Receive a chunk of data
        ssize_t chunk_bytes_received = read(sd, buffer, CHUNK_SIZE);

        if (chunk_bytes_received == -1) // Check if receiving a chunk failed.
        {
            fprintf(stderr, "Receiving file chunk failed\n");
            close(sd);
            fclose(file);
            exit(EXIT_FAILURE);
        }
        else if (chunk_bytes_received == 0)
        {
            break; // End of file reached, exit the loop.
        }

        if (strcmp(buffer, "file DNE\n") == 0) // Check if the response indicates that the file does not exist.
        {
            fprintf(stderr, "File does not exist on the server\n");
            close(sd);
            fclose(file);
            exit(EXIT_FAILURE);
        }

        fwrite(buffer, 1, chunk_bytes_received, file); // Write the received chunk to the file.
        total_bytes_received += chunk_bytes_received; // Update the total bytes received.
    }

    fclose(file); // Close the file.
    close(sd); // Close the socket.

    // Handling errors and exiting
    if (total_bytes_received > 0)
    {
        printf("File received successfully (%zu bytes).\n", total_bytes_received);
    }
    else
    {
        fprintf(stderr, "File transfer failed.\n");
    }

    return 0;
}
