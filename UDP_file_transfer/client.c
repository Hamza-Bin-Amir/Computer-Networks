#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

struct pdu {
    char type;
    char data[100];
};



int main(int argc, char **argv) {
    char *host = "localhost";
    int port = 3000;
    struct hostent *phe;
    struct sockaddr_in sin;
    int s, n;

    switch (argc) {
        case 1:
            break;
        case 2:
            host = argv[1];
        case 3:
            host = argv[1];
            port = atoi(argv[2]);
            break;
        default:
            fprintf(stderr, "usage: UDPfileclient [host [port]]\n");
            exit(1);
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (phe = gethostbyname(host)) {
        memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
    } else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
        fprintf(stderr, "Can't get host entry\n");
        exit(1);
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        fprintf(stderr, "Can't create socket\n");
        exit(1);
    }

    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        fprintf(stderr, "Can't connect to %s %s\n", host, "FileServer");
        exit(1);
    }

    char filename[100];

    while (1) {
        printf("Enter a filename to download (or type 'EXIT' to end): ");
        fgets(filename, sizeof(filename), stdin);
        filename[strcspn(filename, "\n")] = '\0'; // Remove trailing newline

        if (strcmp(filename, "EXIT") == 0) {
            break; // User requested to exit the program
        }

        sendFilename(s, filename);
        printf("Sent filename: %s\n", filename);
        printf("Downloading file: %s\n", filename);

        if (receiveFile(s, filename) < 0) {
            fprintf(stderr, "Failed to receive file: %s\n", filename);
        } else {
            printf("File downloaded: %s\n", filename);
        }
    }

    close(s);
    exit(0);
}


int sendFilename(int s, const char *filename) {
    struct pdu filename_pdu;
    filename_pdu.type = 'C'; // FILENAME PDU
    strncpy(filename_pdu.data, filename, sizeof(filename_pdu.data));
    send(s, &filename_pdu, sizeof(filename_pdu), 0);
}

int receiveFile(int s, const char *filename) {
    struct pdu data_pdu;
    FILE *file = fopen(filename, "wb");

	
    while (1) {
        ssize_t receivedBytes = recv(s, &data_pdu, sizeof(data_pdu), 0);
	printf("Recieved %zd bytes\n", receivedBytes);	
        if (data_pdu.type == 'F') {
            break; // Final PDU received, the file transfer is complete
        }
	if (data_pdu.type == 'E') {
	    printf("File doesn't exist on server");
            break; // Error received, exit
        }

        size_t writtenBytes = fwrite(data_pdu.data, 1, receivedBytes-1, file);
    }

    fclose(file);
    return 0;
}
