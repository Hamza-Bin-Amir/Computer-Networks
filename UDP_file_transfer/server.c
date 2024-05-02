#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>

struct pdu {
    char type;
    char data[100];
};

int main(int argc, char *argv[]) {
    struct sockaddr_in fsin;
    char buf[100];
    char *pts;
    int sock;
    int alen;
    struct sockaddr_in sin;
    int s, type;
    int port = 3000;

    switch (argc) {
        case 1:
            break;
        case 2:
            port = atoi(argv[1]);
            break;
        default:
            fprintf(stderr, "Usage: %s [port]\n", argv[0]);
            exit(1);
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        fprintf(stderr, "can't create socket\n");
        exit(1);
    }

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        fprintf(stderr, "can't bind to %d port\n", port);
        exit(1);
    }

    alen = sizeof(fsin);

    while (1) {
        if (recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&fsin, &alen) < 0) {
            fprintf(stderr, "recvfrom error\n");
        }

        if (buf[0] == 'C') {
            // Filename PDU received, the client wants to download a file
            char filename[100];
            strncpy(filename, buf + 1, sizeof(filename));
            printf("Sending file: %s\n", filename);
            sendFile(s, filename, fsin);
        }
    }
}


int sendFile(int s, const char *filename, struct sockaddr_in fsin) {
    struct pdu data_pdu;
    data_pdu.type = 'C'; // FILENAME PDU
    strncpy(data_pdu.data, filename, sizeof(data_pdu.data));

    data_pdu.type = 'D'; // Data PDU

    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
	data_pdu.type = 'E'; // Error PDU
	printf("Failed to open file\n");
	ssize_t sentBytes = sendto(s, &data_pdu, sizeof(data_pdu.type), 0, (struct sockaddr *)&fsin, sizeof(fsin));        return 0;
    }

    ssize_t bytesRead;
    while ((bytesRead = fread(data_pdu.data, 1, sizeof(data_pdu.data), file)) > 0) {
	printf("Read %zd bytes\n", bytesRead);
        ssize_t sentBytes = sendto(s, &data_pdu, bytesRead+1, 0, (struct sockaddr *)&fsin, sizeof(fsin));
        printf("Sent %zd bytes\n", sentBytes);
    }

    // Send a final PDU to indicate the end of the file
    data_pdu.type = 'F'; // Final PDU
    ssize_t sentBytes = sendto(s, &data_pdu, sizeof(data_pdu.type), 0, (struct sockaddr *)&fsin, sizeof(fsin));
    printf("Done sending file\n");

    fclose(file);
    return 0;
}


// Function to deregister content from the list of content and shift the whole list so there's no empty space in the middle
void deregister_content(struct pdu recv_pdu, struct pdu send_pdu, content_tracker content_list[], int *list_size, struct sockaddr_in fsin, int s) {
    int type;
    char peer_name[10];
	bzero(peer_name, 10);
    char content_name[10];
    bzero(content_name, 10);
	int foundIndex = -1;

	// Parse PDU into components
    for (int i = 0; i < sizeof(recv_pdu.data); i++) {
        if (i < 10) { // 0 - 9
            peer_name[i] = recv_pdu.data[i];
        } else if (i < 20) { // 10 - 19
            content_name[i - 10] = recv_pdu.data[i];
        }
    }
	
	printf("%s\n", peer_name);
	
	for (int i = 0; i < *list_size; i++) {
        if (strcmp(content_list[i].content_name, content_name) == 0 && strcmp(content_list[i].peer_name, peer_name) == 0) {
            foundIndex = i;
            break; // Exit the loop when found
        }
    }

	if ((foundIndex >= 0) && !(foundIndex < *list_size - 1)) {
		//shift everything right
		for (int i = foundIndex; i < *list_size - 1; i++) {
			memset(&content_list[i], '\0', sizeof(content_list[i+1]));
			strcpy(content_list[i].content_name, content_list[i + 1].content_name);
			strcpy(content_list[i].peer_name, content_list[i + 1].peer_name);
			content_list[i].host = content_list[i + 1].host;
			content_list[i].port = content_list[i + 1].port;
		}
		send_pdu.type = 'A';
        bzero(send_pdu.data, 100);
        strcpy(send_pdu.data, peer_name);
        (*list_size)--;
	} else if ((foundIndex >= 0) && (foundIndex < *list_size - 1)) {
		send_pdu.type = 'A';
        bzero(send_pdu.data, 100);
        strcpy(send_pdu.data, peer_name);
        (*list_size)--;
	} else {
        send_pdu.type = 'E';
        bzero(send_pdu.data, 100);
        strcpy(send_pdu.data, "Error, nothing to remove");
    }

    sendto(s, &send_pdu, sizeof(send_pdu.data) + 1, 0, (struct sockaddr *)&fsin, sizeof(fsin));
}

    pduSend.type = 'O';
    memset(pduSend.data, '\0', sizeof(pduSend.data));

    int pduCount = (endOfList + 9) / 10; // Calculate the number of PDUs needed

    for (int pduIndex = 0; pduIndex < pduCount; pduIndex++) {
        int contentIndexStart = pduIndex * 10;
        int contentIndexEnd = contentIndexStart + 10;
        
        // Ensure we don't go beyond the end of the list
        if (contentIndexEnd > endOfList) {
            contentIndexEnd = endOfList;
        }
        
        for (i = contentIndexStart; i < contentIndexEnd; i++) {
            memcpy(pduSend.data + (i - contentIndexStart) * 10, listOfContent[i].contentName, 10);
        }
        
        sendto(s, &pduSend, sizeof(pduSend.data) + 1, 0, (struct sockaddr*)&fsin, sizeof(fsin));
    }