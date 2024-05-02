#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include <ctype.h>

#define BUFLEN 100

// PDU for data transmission
struct pdu {
    char type;
    char data[100];
};

// Struct to store the data needed for content
typedef struct {
    char content_name[10];
    char peer_name[10];
    int host;
    int port;
    int dnld_num;
} content_tracker;

// Function to convert an IP address format to an integer (Source: https://gist.github.com/jayjayswal/fc435fe261af9e45ccaf)
int stohi(char *ip) {
    char c;
    c = *ip;
    unsigned int integer;
    int val;
    int i, j = 0;
    for (j = 0; j < 4; j++) {
        if (!isdigit(c)) { //first char is 0
            return (0);
        }
        val = 0;
        for (i = 0; i < 3; i++) {
            if (isdigit(c)) {
                val = (val * 10) + (c - '0');
                c = *++ip;
            } else
                break;
        }
        if (val < 0 || val > 255) {
            return (0);
        }
        if (c == '.') {
            integer = (integer << 8) | val;
            c = *++ip;
        } else if (j == 3 && c == '\0') {
            integer = (integer << 8) | val;
            break;
        }
    }
    if (c != '\0') {
        return (0);
    }
    return (htonl(integer));
}

// Function prototypes
void register_content(struct pdu recv_pdu, struct pdu send_pdu, content_tracker content_list[], int *list_size, struct sockaddr_in fsin, int s);
void search_content(struct pdu recv_pdu, struct pdu send_pdu, content_tracker content_list[], int list_size, struct sockaddr_in fsin, int s);
void list_content(struct pdu send_pdu, content_tracker content_list[], int list_size, struct sockaddr_in fsin, int s);
void deregister_content(struct pdu recv_pdu, struct pdu send_pdu, content_tracker content_list[], int *list_size, struct sockaddr_in fsin, int s);

int main(int argc, char *argv[]) {

    struct sockaddr_in fsin; /* the from address of a client */
    char buf[100];           /* "input" buffer; any size > 0 */
    char *pts;
    int sock; /* server socket */
    int alen; /* from-address length */
    struct sockaddr_in sin; /* an Internet endpoint address         */
    int s, type;            /* socket descriptor and socket type    */
    int port = 3000;
	
	struct pdu recv_pdu;
    struct pdu send_pdu;
    content_tracker content_list[10];
    int list_size = 0;
    int i;

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

    /* Allocate a socket */
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
	
	printf("|**************************|\n");
	printf("|* Content Server Started *|\n");
	printf("|**************************|\n");


	//Infinite loop to recive content requests
	while (1) {

		if (recvfrom(s, &recv_pdu, sizeof(recv_pdu.data) + 1, 0, (struct sockaddr *)&fsin, &alen) < 0){
			fprintf(stderr, "recvfrom error\n");
		}

		if (recv_pdu.type == 'R') {
			printf("Recived register request from:\n");
			register_content(recv_pdu, send_pdu, content_list, &list_size, fsin, s);
		} else if (recv_pdu.type == 'S') {
			printf("Recived search request from:\n");
			search_content(recv_pdu, send_pdu, content_list, list_size, fsin, s);
		} else if (recv_pdu.type == 'O') {
			printf("Recived listing request\n");
			list_content(recv_pdu, content_list, list_size, fsin, s);
		} else if (recv_pdu.type == 'T') {
			printf("Recived de-registering request from:\n");
			deregister_content(recv_pdu, send_pdu, content_list, &list_size, fsin, s);
		} else {
			fprintf(stderr, "Unknown request from %d port\n", port);
		}
		printf("Request complete\n");
	}
    return 0;
}

// Function to handle registering content
void register_content(struct pdu recv_pdu, struct pdu send_pdu, content_tracker content_list[], int *list_size, struct sockaddr_in fsin, int s) {
    char content_address[16];
    bzero(content_address, 16);
    inet_ntop(AF_INET, &fsin.sin_addr, (char *)content_address, sizeof(content_address));
    int content_case;
    char peer_name[10];
	bzero(peer_name, 10);
    char content_name[10];
	bzero(content_name, 10);
    char port[6];
    bzero(port, 6);

	
    // Parse PDU into components
    for (int i = 0; i < sizeof(recv_pdu.data); i++) {
        if (i < 10) { // 0 - 9
            peer_name[i] = recv_pdu.data[i];
        } else if (i < 20) { // 10 - 19
            content_name[i - 10] = recv_pdu.data[i];
        } else if (i < 26) { // 20 - 25
            port[i - 20] = recv_pdu.data[i];
        }
    }
	
	int host_content = stohi(content_address);
    int port_content = atoi(port);
	
	printf("%s\n", peer_name);
	
	// Check if content is already registered by the same peer with the same port
    for (int i = 0; i < *list_size; i++) {
		content_case = 0; // Not registered
        if (strcmp(content_list[i].content_name, content_name) == 0) {
            if (strcmp(content_list[i].peer_name, peer_name) == 0 && (content_list[i].port == port_content)) {
                content_case = 2; // Content already registered under the same peer with the same port
                break;
            } else if (strcmp(content_list[i].peer_name, peer_name) == 0) {
                content_case = 1; // Content already registered with the same peer name diff port, peer name conflict
                break;
            }
        }
    }
    // Add content to the list of available content on the content server, if it passes the error check
    if (content_case == 0) {
		send_pdu.type = 'A';
		bzero(send_pdu.data, 100);
        memset(&content_list[*list_size], '\0', sizeof(content_list[*list_size]));
        strcpy(content_list[*list_size].content_name, content_name);
        strcpy(content_list[*list_size].peer_name, peer_name);
        content_list[*list_size].host = host_content;
        content_list[*list_size].port = port_content;
        (*list_size)++;
        strcpy(send_pdu.data, peer_name);
    } else if (content_case == 1){
		send_pdu.type = 'E';
        bzero(send_pdu.data, 100);
        strcpy(send_pdu.data, "This peer name is already in use, please close the server and open it under a new peer name\n");
	} else {
        send_pdu.type = 'E';
        bzero(send_pdu.data, 100);
        strcpy(send_pdu.data, "This content is already registered by this peer\n");
    }

    sendto(s, &send_pdu, sizeof(send_pdu.data) + 1, 0, (struct sockaddr *)&fsin, sizeof(fsin));
}



// Function to handle client searching for content
void search_content(struct pdu recv_pdu, struct pdu send_pdu, content_tracker content_list[], int list_size, struct sockaddr_in fsin, int s) {
    char peer_name[10];
	bzero(peer_name, 10);
    char content_address[10];
	bzero(content_address, 10);
    int content_dnld_index = -1;
	char TCP_ip[10];
	bzero(TCP_ip, 5);
    char TCP_port[6];
	bzero(TCP_port, 6);

    // Parse PDU
    for (int i = 0; i < sizeof(recv_pdu.data); i++) {
        if (i < 10) { // 0 - 9
            peer_name[i] = recv_pdu.data[i];
        } else if (i < 20) { // 10 - 19
            content_address[i - 10] = recv_pdu.data[i];
        }
    }
	
	printf("%s\n", peer_name);
	
    // Search the content list and find the index that has the lowest download count
	for (int i = 0; i < list_size; i++) {
		if (strcmp(content_address, content_list[i].content_name) == 0) {
			if (content_dnld_index == -1 || content_list[i].dnld_num <= content_list[content_dnld_index].dnld_num) {
				content_dnld_index = i;
			}
		}
	}

	printf("Sent from: %s\n", content_list[content_dnld_index].peer_name); // List which peer the download info is from

    // Send TCP port and IP to client when content is found, otherwise send error that content was not founds
    if (content_dnld_index != -1) {
        send_pdu.type = 'S';
        bzero(send_pdu.data, 100);
        strcpy(send_pdu.data, peer_name);
        snprintf(TCP_ip, sizeof(TCP_ip), "%d", content_list[content_dnld_index].host);
		snprintf(TCP_port, sizeof(TCP_port), "%d", content_list[content_dnld_index].port);
        memcpy(send_pdu.data + 10, TCP_port, 5);
        memcpy(send_pdu.data + 15, TCP_ip, 10);
		content_list[content_dnld_index].dnld_num++;
    } else {
        send_pdu.type = 'E';
        bzero(send_pdu.data, 100);
        strcpy(send_pdu.data, "The requested content was not found\n");
    }

    sendto(s, &send_pdu, sizeof(send_pdu.data) + 1, 0, (struct sockaddr *)&fsin, sizeof(fsin));
}

// Function to send all of the currently registered content
void list_content(struct pdu send_pdu, content_tracker content_list[], int list_size, struct sockaddr_in fsin, int s) {    
    send_pdu.type = 'O';
    char *data_ptr;
    int pdu_data_size = sizeof(send_pdu.data);

	// If there is content in the list
    if(list_size > 0){
    	for (int i = 0; i < list_size; i++) {
    	    data_ptr = &send_pdu.data[(i % 10) * 10]; // Point to the correct offset in the PDU data
    	    strncpy(data_ptr, content_list[i].content_name, 10); // Copy the content name
    	    
    	    // If PDU is filled up or at the end of the list, send it
    	    if ((i % 10 == 9) || (i == list_size - 1)) {
    	        if (i == list_size - 1) {
    	            // Zero out the remaining part of the PDU data
    	            memset(data_ptr + strlen(content_list[i].content_name), '\0', pdu_data_size - ((i % 10) * 10) - strlen(content_list[i].content_name));
    	        }
    	        
    	        if (sendto(s, &send_pdu, pdu_data_size+1, 0, (struct sockaddr*)&fsin, sizeof(fsin)) < 0) {
    	            perror("sendto failed");
    	            return;
    	        }
    	        
    	        // Reset the PDU data
    	        memset(send_pdu.data, '\0', pdu_data_size);
    	    }
    	}
	} else {
		send_pdu.type = 'E';
        bzero(send_pdu.data, 100);
        strcpy(send_pdu.data, "Error, list is empty");
		if (sendto(s, &send_pdu, pdu_data_size+1, 0, (struct sockaddr*)&fsin, sizeof(fsin)) < 0) {
			perror("sendto failed");
		}
	}
}



// Function to deregister content from the list of content
void deregister_content(struct pdu recv_pdu, struct pdu send_pdu, content_tracker content_list[], int *list_size, struct sockaddr_in fsin, int s) {
    int type;
    char peer_name[10];
	bzero(peer_name, 10);
    char content_name[10];
    bzero(content_name, 10);
	int found_index = -1;

	// Parse PDU into components
    for (int i = 0; i < sizeof(recv_pdu.data); i++) {
        if (i < 10) { // 0 - 9
            peer_name[i] = recv_pdu.data[i];
        } else if (i < 20) { // 10 - 19
            content_name[i - 10] = recv_pdu.data[i];
        }
    }
	
	printf("%s\n", peer_name);
	
	// Find content index in content list 
	for (int i = 0; i < *list_size; i++) {
        if (strcmp(content_list[i].content_name, content_name) == 0 && strcmp(content_list[i].peer_name, peer_name) == 0) {
            found_index = i;
            break;
        }
    }
	
	// Remove the content by shifting the list to the right, overwriting the content in the list, then decrease list by 1. If nothing found send error.
	if (found_index >= 0) {
		// Shift everything right
		for (int i = found_index; i < *list_size - 1; i++) {
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
	} else {
        send_pdu.type = 'E';
        bzero(send_pdu.data, 100);
        strcpy(send_pdu.data, "Error, nothing to remove");
    }

    sendto(s, &send_pdu, sizeof(send_pdu.data) + 1, 0, (struct sockaddr *)&fsin, sizeof(fsin));
}