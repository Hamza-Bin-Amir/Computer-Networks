/* P2P_client.c - main */
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>                                                        
#include <netdb.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>    
#include <unistd.h>

#define BUFSIZE 64

#define MSG "Any Message \n"


/*------------------------------------------------------------------------
 * main - UDP client for TIME service that prints the resulting time
 *------------------------------------------------------------------------
 */
struct pdu{
char type;
char data[100];
};

struct pduC{
char type;
char data[1400];
};

// Global Variables
char peer_name[10];
char content_name_arr[10][10]; // Store local content names up to 10
char content_port_arr[10][6]; // Store local content ports
int local_content_count; // Store number of local content saved

// UDP connection variables
int socket_descriptor_udp;

// TCP socket variables
int socket_descriptor_tcp, connection_handler_tcp;
struct sockaddr_in socketArray[5];
int tcp_port_arr[5] = {0};
int highest_socket_descriptor = 0;

// Connection info
    char *host = "localhost";
    int port = 3000;
    struct hostent *phe; /* pointer to host information entry */


void add_local_content(char content_name[], char port[], int socket, struct sockaddr_in server){

    strcpy(content_name_arr[local_content_count], content_name);
    strcpy(content_port_arr[local_content_count], port);
    tcp_port_arr[local_content_count] = socket;
    socketArray[local_content_count] = server;

    if(socket > highest_socket_descriptor){
        highest_socket_descriptor = socket;
    }

    local_content_count++;
}

int PDU_read_O_type(){
    struct pdu send_pdu;
    struct pdu receive_pdu;
    memset(&receive_pdu, '\0', sizeof(receive_pdu.data) + 1);
    memset(&send_pdu, '\0', sizeof(send_pdu.data) + 1);
    send_pdu.type = 'O';
    read(socket_descriptor_udp, &receive_pdu, sizeof(receive_pdu.data) + 1);
    int a;
    for(a = 0; a < sizeof(receive_pdu.data); a+=10){
        if(receive_pdu.data[a] == '\0'){
            return 1;
        }else{
            printf("%d: %s\n", a/10, receive_pdu.data + a);
        }
    }
    return -1;
}
 
void content_registration(char content_name[]){
    // Variable Declarations
    FILE *file_ptr;
    int i, remaining_file_data;
    struct pdu send_pdu;
    memset(&send_pdu, '\0', sizeof(send_pdu));
    struct pdu receive_pdu;
    memset(&receive_pdu, '\0', sizeof(send_pdu));

    // Open the local file for reading
    file_ptr = fopen(content_name, "r");
    if(file_ptr == NULL){
    printf("File you tried to register does not exist locally\n");
    return;
    }

    // TCP Connection setup
    struct sockaddr_in server, client;
    struct pdu incomingPdu;
    char tcp_host[5];
    char tcp_port[6];
    int alen, read_len;

    // Create a stream socket
    if ((socket_descriptor_tcp = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Cannot create a socket\n");
    exit(1);
    }

    // Bind the socket to a port
    bzero((char *)&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(0);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(socket_descriptor_tcp, (struct sockaddr *)&server, sizeof(server));
    alen = sizeof(struct sockaddr_in);
    getsockname(socket_descriptor_tcp, (struct sockaddr*)&server, &alen);

    sprintf(tcp_host, "%d", server.sin_addr.s_addr);
    sprintf(tcp_port, "%d", htons(server.sin_port));

    // Prepare the Registration PDU
    memset(&send_pdu, '\0', sizeof(send_pdu));
    memcpy(send_pdu.data, peer_name, 10);
    memcpy(send_pdu.data + 10, content_name, 10);
    memcpy(send_pdu.data + 20, tcp_port, sizeof(tcp_port));
    send_pdu.type = 'R';

    // Print and send the Registration PDU
    printf("Sending the following Registration PDU:\n");
    printf("Registration: name: %s\n", content_name);
    printf("Registration: Port: %s\n\n", tcp_port);
    write(socket_descriptor_udp, &send_pdu, sizeof(send_pdu.data) + 1);

    // Read the server's response
    int size = 0;
    size = read(socket_descriptor_udp, &receive_pdu, sizeof(receive_pdu.data) + 1);
    char errorMsg[size];
    bzero(errorMsg, size);

    // Process the server's response based on the PDU type
    switch(receive_pdu.type){

        case 'E':
            //Handle error response
            for(i = 0; i < size; i++){
                errorMsg[i] = receive_pdu.data[i];
            }
            printf("%s\n", errorMsg);
            break;

        case 'A':
            // Successful registration response
            printf("Content was registered\n\n");
            // Add local content information
            add_local_content(content_name, tcp_port, socket_descriptor_tcp, server);

            // Set up TCP server to handle content downloads
            listen(socket_descriptor_tcp,5);
            switch(fork()){
                case 0:
                    // Child process handles content download requests
                while(1){

                    int client_len = sizeof(client);
                    char fileName[10];
                    connection_handler_tcp = accept(socket_descriptor_tcp, (struct sockaddr*)&client, &client_len);
                    memset(&receive_pdu, '\0', sizeof(receive_pdu.data) + 1);
                    memset(&fileName, '\0', sizeof(fileName));
                    read_len = read(connection_handler_tcp, &receive_pdu, sizeof(receive_pdu.data) + 1);
                    if(receive_pdu.type == 'D'){
                   
                        memcpy(fileName, receive_pdu.data + 10, 10);
                        
                        printf("FileName: %s", fileName);
                        FILE *file_ptr;
                        file_ptr = fopen(fileName, "rb");
                        if(file_ptr == NULL){
                            printf("File was not found locally\n");
                        }
                        else{
                            char contentToSend[1401];
                            memset(contentToSend, '\0', sizeof(contentToSend));
                            contentToSend[0] = 'C';

                        while(fgets(contentToSend + 1, sizeof(contentToSend) - 1, file_ptr) > 0){
                            write(connection_handler_tcp, contentToSend, sizeof(contentToSend));
                            }
   
                        }

                    }
                    
                
                   
                    fclose(file_ptr);
                    close(connection_handler_tcp);
                    
                    }
            
                default:
                    break;
            }
        

        break;

        default:
            break;
}

}

// Index server will send back a corresponding S or E type pdu after this function is called.
void content_address_get(char content_name[]){
    printf("Looking for content\n");
    struct pdu receive_pdu;
    struct pdu send_pdu;
    memset(&receive_pdu, '\0', sizeof(receive_pdu.data) + 1);
    memset(&send_pdu, '\0', sizeof(send_pdu.data) + 1);

    send_pdu.type = 'S';
    memcpy(send_pdu.data, peer_name, sizeof(peer_name));
    memcpy(send_pdu.data + 10, content_name, 10);
    //printf("ContentName sent: %s", send_pdu.data + 10);
    write(socket_descriptor_udp, &send_pdu, sizeof(send_pdu.data) + 1);
}

void remove_from_local_content(char content_name[]){
    int x;
    int set_flag = 0;

    for(x = 0; x < local_content_count; x++){
        if(strcmp(content_name_arr[x], content_name) == 0){
            set_flag = 1;
            memset(content_name_arr[x], '\0', sizeof(content_name_arr[x]));
            memset(content_name_arr[x], '\0', sizeof(content_port_arr[x]));
            tcp_port_arr[x] = 0;

        while(x < local_content_count - 1){
            strcpy(content_name_arr[x], content_name_arr[x + 1]);
            strcpy(content_port_arr[x], content_port_arr[x + 1]);
            tcp_port_arr[x] = tcp_port_arr[x + 1];
            socketArray[x] = socketArray[x + 1];
            x++;
        }
        break;
    }
    }
    if(set_flag == 0){
        printf("There was no content found to delete\n");
    }else{
        local_content_count--;
    }
}

void print_commands(){
	printf("Commands:\n");
    printf("D-Download Content\n");
    printf("L-List Local Content\n");
    printf("O-List all the On-line Content\n");
    printf("R-Content Registration\n");
    printf("T-Content Deregistration\n");
    printf("Q-Quit\n");
}

void content_download(char content_name[], char address[]){

    struct sockaddr_in server;
    struct hostent * hp;
    char *serverHost = "0";
    char port_str_s[6];

    int port_s;
    int dnld_socket;
    int y;

    for(y = 0; y < 5; y++){
        port_str_s[y] = address[y];
    }

    port_s = atoi(port_str_s);
    /* Create a stream socket   */  
    if ((dnld_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Cannot create a socket\n");
        exit(1);
    }

    bzero((char *)&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port_s);
    if (hp = gethostbyname(serverHost)) 
      bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
    else if ( inet_aton(host, (struct in_addr *) &server.sin_addr) ){
      fprintf(stderr, "Cannot get the server address\n");
      exit(1);
    }

    /* Connecting to the server */
    if (connect(dnld_socket, (struct sockaddr *)&server, sizeof(server)) == -1){
      fprintf(stderr, "Cannot connect");
      exit(1);
    }

    // PDU Creation
    struct pduC PDUC_receive;
    struct pdu PDUD_send;
    memset(&PDUC_receive, '\0', sizeof(PDUC_receive.data) + 1);
    memset(&PDUD_send, '\0', sizeof(PDUD_send.data) + 1);

    PDUD_send.type = 'D';
    memcpy(PDUD_send.data, peer_name, 10);
    memcpy(PDUD_send.data + 10, content_name, 90);

    write(dnld_socket, &PDUD_send, sizeof(PDUD_send.data) + 1);

    FILE *file_ptr;
    struct pduC c_type_PDU;
    int received = 0;

    file_ptr = fopen(content_name, "w+");
    memset(&c_type_PDU, '\0', sizeof(c_type_PDU));
    int size;
    while((size = read(dnld_socket, (struct pdu*)&c_type_PDU, sizeof(c_type_PDU))) > 0){
        if(c_type_PDU.type == 'C'){
            fprintf(file_ptr, "%s", c_type_PDU.data);
            received = 1;
        } else if(c_type_PDU.type == 'E'){
            printf("This is an Error; could not start download");
        }else{
            memset(&c_type_PDU, '\0', sizeof(c_type_PDU));
        }
    }
    fclose(file_ptr);
    if(received){
        content_registration(content_name);
    }

}

void index_server_content_view(){
    int check = 0;
    struct pdu send_pdu;
    struct pdu receive_pdu;

    memset(&receive_pdu, '\0', sizeof(receive_pdu.data) + 1);
    memset(&send_pdu, '\0', sizeof(send_pdu.data) + 1);
    send_pdu.type = 'O';

    write(socket_descriptor_udp, &send_pdu, sizeof(send_pdu.data) + 1);

    printf("This is inside the Index Server:\n");
    while(PDU_read_O_type() == -1){
    }

    printf("\n");
}

void view_local_content(){
    printf("The Local Registered Content:\n");
    printf("#\tContent\t\t\tPort\t\tSocket\n");
    
    int z;
    for(z = 0; z < local_content_count; z++){
        printf("%d\t%-10s\t\t%s\t\t%d\n", z, content_name_arr[z], content_port_arr[z], tcp_port_arr[z]);
    }
    printf("\n");
}

void content_deregistration(char content_name[]){
    struct pdu receive_pdu;
    struct pdu send_pdu;
    memset(&receive_pdu, '\0', sizeof(receive_pdu.data) + 1);
    memset(&send_pdu, '\0', sizeof(send_pdu.data) + 1);
    send_pdu.type = 'T';
    memcpy(send_pdu.data, peer_name, sizeof(peer_name));
    memcpy(send_pdu.data + 10, content_name, 10);
    write(socket_descriptor_udp, &send_pdu, sizeof(send_pdu.data) + 1);
    remove_from_local_content(content_name);

    read(socket_descriptor_udp, &receive_pdu, sizeof(receive_pdu.data) + 1);

}

int main(int argc, char **argv)
{
    switch (argc) {
        case 1:
            break;
        case 2:
            host = argv[1];
            break;
        case 3:
            host = argv[1];
            port = atoi(argv[2]);
            break;
        default:
            fprintf(stderr, "USAGE: UDPtime [host [port]]\n");
            exit(1);
    }

	struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
            sin.sin_family = AF_INET;                                                                
            sin.sin_port = htons(port);
                                                                                        
        /* Map host name to IP address, allowing for dotted decimal */
            if ( phe = gethostbyname(host) ){
                    memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
            }
            else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
                 fprintf(stderr, "Cannot get host entry \n");
                                                                                
        /* Allocate a socket */
            socket_descriptor_udp = socket(AF_INET, SOCK_DGRAM, 0);
            if (socket_descriptor_udp < 0)
                fprintf(stderr, "Cannot create the socket \n");

                                                                                
        /* Connect the socket */
            if (connect(socket_descriptor_udp, (struct sockaddr *)&sin, sizeof(sin)) < 0)
                fprintf(stderr, "Cannot connect to %s \n", host);


    printf("---------------------\n");
    printf("Welcome to our Peer-to-Peer Application\n");
    printf("---------------------\n");
    printf("Please insert your peer name:\n");
    while(read(0, peer_name, sizeof(peer_name)) > 10){
    printf("Keep your username limited to a maximum of nine characters");
    }
    peer_name[strlen(peer_name) - 1] = '\0';
    printf("Hello %s \n", peer_name);

    //Variables for main

    char user_request[2];
    char input_from_user[10];
    struct pdu receive_pdu;
    int user_quit_request = 0;
    int b;
    char address[6];
    char IP_addrress[10];

    while(user_quit_request != 1){
        memset(&receive_pdu, '\0', sizeof(receive_pdu.data) + 1);
		print_commands();
        read(0, user_request, 2);
        bzero(address, sizeof(address));
        bzero(IP_addrress, sizeof(IP_addrress));

        switch(user_request[0]){
            case 'R':
                printf("Enter the name of the content you want to register (MAX nine characters):\n");
                scanf("%10s", input_from_user);
                content_registration(input_from_user);
                break;

            case 'T':
                printf("Enter the name of the content you want to de-register (MAX nine characters):\n");
                scanf("%10s", input_from_user);
                content_deregistration(input_from_user);
                break;

            case 'L':
                view_local_content();
                break;

            case 'D':
                printf("Enter the name of the content you wish to download:\n");
                scanf("%10s", input_from_user);
                content_address_get(input_from_user);
                read(socket_descriptor_udp, &receive_pdu, sizeof(receive_pdu.data) + 1);

                switch(receive_pdu.type){
                    case 'E':
                        printf("Error PDU Received with the following:\n");
                        printf("%s\n", receive_pdu.data);
                        break;
                    case 'S':
                        memcpy(address, receive_pdu.data + 10, 5);
                        memcpy(IP_addrress, receive_pdu.data + 15, 10);
                        printf("Download: Peer IP Address: %s\n", IP_addrress);
                        printf("Download: Peer Port number: %s\n", address);
                        content_download(input_from_user, address);
                        break;
                }
                break;

            case 'O':
                index_server_content_view();
                break;
            case 'Q':{
		int content_counter = local_content_count;
                for(b = 0; b < content_counter; b++){
                    content_deregistration(content_name_arr[b]);
                }
                user_quit_request = 1;
                break;
                }
            default:
                printf("Not a valid option! Choose from one of the following valid options:\n");


        }


    }

    close(socket_descriptor_udp);
    close(socket_descriptor_tcp);
    exit(0);

}