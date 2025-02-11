// Angel Garcia
// Anmol Virdi
// EECE 446
// Fall 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> // referece beej guide, chapter: 9.13
#include <unistd.h>
#include <ctype.h>
#include <dirent.h> //opendir and readdir

#define FILENAME_SIZE 256
#define BUFFER_SIZE 1024

int lookup_and_connect(const char *host, const char *service);
int recvall(int s, char *buf, int size);
int sendall(int s, const char *buf, int *len);
void send_join(int s, int peer_id);
void publish(int s);
void search(int s, const char *file);
void fetch(int s);

int main(int argc, char *argv[]) {
        const char* host = argv[1];
        const char* port = argv[2];
        int peer_id = atoi(argv[3]);
        char argument[10] = "";
        char file[256];

        int s = lookup_and_connect(host, port);
        if (s < 0) {
                fprintf(stderr, "Failed to connect");
                exit(1);
        }
        while (strcmp(argument, "EXIT") != 0) {
            printf("Enter a command: ");
            scanf("%s", argument);
            for (int i =0; argument[i] != '\0'; i++) {
                    argument[i] = toupper(argument[i]);
            }
            if (strcmp(argument, "JOIN") == 0) {
                    send_join(s, peer_id);
            } else if (strcmp(argument, "PUBLISH") == 0) {
                    publish(s);
            } else if (strcmp(argument, "SEARCH") == 0) {
                    printf("Enter a file name: ");
                    scanf("%s", file);
                    search(s, file);
            } else if (strcmp(argument, "FETCH") == 0) {
                fetch(s);
            }
        }

        // sends the JOIN request
        //send_join(s, peer_id);
}

void send_join(int s, int peer_id) {
        uint8_t action = 0;
        uint32_t network_byte_order = htonl(peer_id);
        // reference beej guide 9.12: help with conversion of host byte order to network byte order

        // Join request set up
        // 1B for action, 4B for peer ID
        char join_request[5];

        // first byte of join_action is for the action = 0
        join_request[0] = action;

        // copies the peer ID to the next section of the packet
        // how memcpy works: (dest, source, sizeof(source)
        // reference: Building and Parsing Packets provided in canvas
        // reference: https://sternumiot.com/iot-blog/memcpy-c-function-examples-and-best-practices/
        memcpy(&join_request[1], &network_byte_order, sizeof(network_byte_order));

        // sends the pkt
        send(s, join_request, sizeof(join_request), 0);
}

void publish(int s) {
        uint8_t action = 1;
        uint32_t count = 0;

        // No larger than 1200 bytes, -5 for message format 1B 4B 
        char file_name[1200 - 5];
        // pointer for files
        char *ptr = file_name;

        // open SharedFiles directory
        DIR *dir = opendir("SharedFiles");

        if (dir == NULL) {
                perror("Could not open directory");
                return;
        }

        struct dirent *dp;

        while ((dp = readdir(dir)) != NULL) {
                size_t name_length = strlen(dp->d_name);

                if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) {
                        continue;
                }

                // copy the file name and end it with NULL 
                strcpy(ptr, dp->d_name);
                ptr += name_length;
                *ptr = '\0';
                ptr += 1;
                
                // increment for each file found in directory
                count += 1;
        }
        // exit out of directory
        closedir(dir);

        // network byte order for count
        uint32_t network_byte_order = htonl(count);

        // 1B is action, 4B is count, and rest is for the variable which is file names
        char publish[5 + (ptr - file_name)];

        publish[0] = action;

        // use memcpy to copy the count in network byte order for the request
        memcpy(&publish[1], &network_byte_order, sizeof(network_byte_order));

        // use memcpy to copy the names of all the files 
        memcpy(&publish[5], file_name, ptr - file_name);

        int pub_size = 5 + (ptr - file_name);
        // sends the pkt
        send(s, publish, pub_size, 0);
}

void search(int s, const char *file) {
        uint8_t action = 2;
        uint32_t peer_id, peer_ip;
        uint16_t peer_port;

        size_t file_length = strlen(file) + 1;
        char search_recieve[10];
        char search_request[1 + file_length];

        search_request[0] = action;

        memcpy(&search_request[1], file, file_length);

        // Send search request
        send(s, search_request, sizeof(search_request), 0);

        // Recieve bytes from search
        recv(s, search_recieve, sizeof(search_recieve), 0);

        // After recieving the data for the search file, extract peer id, ip, and port
        memcpy(&peer_id, &search_recieve[0], 4);
        memcpy(&peer_ip, &search_recieve[4], 4);
        memcpy(&peer_port, &search_recieve[8], 2);

        // Network byte order
        peer_id = ntohl(peer_id);
        peer_ip = ntohl(peer_ip);
        peer_port = ntohs(peer_port);

        // Check if file was found
        if (peer_id == 0 && peer_ip == 0 && peer_port == 0) {
                printf("File not indexed by registry \n");
        } else {
                
                // Used to corerctly format the IP address
                struct in_addr ip_addr;
                ip_addr.s_addr = peer_ip;
                
                // Can hold valid Ipv4 addresses
                char ip_str[INET_ADDRSTRLEN];
                
                // Convert the ip from binary to string
                inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));
                
                // For some reason my ip address is flipped, so to pass all tests I have to flip it
                uint8_t a,b,c,d;
                sscanf(ip_str, "%hhu.%hhu.%hhu.%hhu", &a,&b,&c,&d);

                char flip[INET_ADDRSTRLEN];
                snprintf(flip, sizeof(flip), "%hhu.%hhu.%hhu.%hhu", d,c,b,a);
                
                printf("File found at\n");
                printf("Peer %u\n", peer_id);
                printf("%s:%u\n", flip, peer_port);
        }
}

// Locate file to find peer with file, connect to peer, fetch and save
void fetch(int s) {
    // buffer to hold the file name
    // user enters the file name
    char filename[FILENAME_SIZE];
    printf("Enter the file name to fetch: ");
    scanf("%s", filename);

    // length of the file name
    int filename_length = strlen(filename) + 1;

    // prep for the search request, 2 for search
    char search_request[1 + filename_length];
    search_request[0] = 2;
    memcpy(&search_request[1], filename, filename_length);

    // size of search request
    int search_request_size = sizeof(search_request);

    // serach request gets sent to peer
    // error incase it fails
    if (send(s, search_request, search_request_size, 0) == -1) {
        perror("SEARCH request failed to send");
        return;
    }

    // buffer to handle the response
    // search_response[10] = 4 bytes peer_id, 4 bytes peer_ip, 2 bytes peer_port 
    char search_response[10];
    if (recv(s, search_response, sizeof(search_response), 0) == -1) {
        perror("SEARCH response not received");
        return;
    }

    // peer_id and peer_ip both 4 bytes, 1 byte = 8 bits
    // peer_port is 2 bytes, 1 byte = 8 bits
    uint32_t peer_id, peer_ip;
    uint16_t peer_port;
    // first 4 bytes of the search_reponse to peer_id
    // next 4 bytes to peer_ip
    // next to 2 bytes peer_port
    memcpy(&peer_id, search_response, 4);
    memcpy(&peer_ip, search_response + 4, 4);
    memcpy(&peer_port, search_response + 8, 2);

    // conversion of netwrok bytes to host bytes order
    peer_id = ntohl(peer_id);
    peer_port = ntohs(peer_port);

    // Check if file was found
    if (peer_id == 0 && peer_ip == 0 && peer_port == 0) {
        printf("File not found on the network.\n");
        return;
    }

    // Used to corerctly format the IP address
    struct in_addr ip_addr;
    ip_addr.s_addr = peer_ip;

    // Can hold valid Ipv4 addresses
    char ip_str[INET_ADDRSTRLEN];

    // Convert the ip from binary to string
    inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%u", peer_port);

    int peer_sock = lookup_and_connect(ip_str, port_str);

    // fetch request buffer, for the fetch request code (3) and filename
    char fetch_request[1 + filename_length];
    fetch_request[0] = 3;
    memcpy(&fetch_request[1], filename, filename_length);

    // gets the size of the fetch request
    int fetch_request_size = sizeof(fetch_request);

    // sends fetch to peer, error if the fetch fails
    if (send(peer_sock, fetch_request, fetch_request_size, 0) == -1) {
        perror("FETCH request failed to send");
        close(peer_sock);
        return;
    }

    // receive reponse code from peer
    uint8_t response_code;
    recv(peer_sock, &response_code, 1, 0);

    // open file to save the data being received, error if the file cant be opened
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("File can't be opened");
        close(peer_sock);
        return;
    }

    // buffer to store the data being received from peer
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    // keep receiving data until it's fully sent
    while ((bytes_received = recv(peer_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    close(peer_sock);
}

// provided and reused from program 1
int lookup_and_connect( const char *host, const char *service ) {
    struct addrinfo hints;
    struct addrinfo *rp, *result;
    int s;

    /* Translate host name into peer's IP address */
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    if ( ( s = getaddrinfo( host, service, &hints, &result ) ) != 0 ) {
        fprintf( stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror( s ) );
        return -1;
    }
    /* Iterate through the address list and try to connect */
    for ( rp = result; rp != NULL; rp = rp->ai_next ) {
        if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
            continue;
        }

        if ( connect( s, rp->ai_addr, rp->ai_addrlen ) != -1 ) {
            break;
        }

        close( s );
    }
    if ( rp == NULL ) {
        perror( "stream-talk-client: connect" );
        return -1;
    }
    freeaddrinfo( result );

    return s;
}