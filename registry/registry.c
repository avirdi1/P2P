// Anmol Virdi
// Angel Garcia
// EECE 446
// Fall 2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MAX_PEERS 5
#define MAX_FILES 10
#define MAX_FILENAME_LEN 100
#define BUFFER_SIZE 1024

// Given struct with an added file count to not exceed ten
struct peer_entry {
    uint32_t id;
    int socket_descriptor;
    char files[MAX_FILES][MAX_FILENAME_LEN];
    int file_count;
    struct sockaddr_in address;
};

struct peer_entry peers[MAX_PEERS];
int peer_count = 0;

int bind_and_listen(const char *port);
void new_connection(int socket_listen, fd_set *master, int *fdmax);
void peer_message(int p_socket, fd_set *master);
void process_command(int p_socket, char *buffer, int nbytes);
void remove_peer(int p_socket, fd_set *master);
void join(int p_socket, char *data);
void publish(int p_socket, char *data, int nbytes);
void search(int p_socket, char *data);

// Using his given tcp_server_select file to incorporate select
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int socket_listen = bind_and_listen(argv[1]);
    if (socket_listen < 0) {
        exit(EXIT_FAILURE);
    }

    fd_set master_set, read_fds;
    FD_ZERO(&master_set);
    FD_SET(socket_listen, &master_set);
    int fdmax = socket_listen;

    printf("Port %s\n", argv[1]);

    while (1) {
        read_fds = master_set;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == socket_listen) {
                    new_connection(socket_listen, &master_set, &fdmax);
                } else {
                    peer_message(i, &master_set);
                }
            }
        }
    }

    return 0;
}

//Returns socket descriptor for the listening socket
int bind_and_listen(const char *port) {
    int socket_listen;
    struct sockaddr_in server_addr;

    if ((socket_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(port));

    // Bind socket
    if (bind(socket_listen, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(socket_listen);
        return -1;
    }

    // Listen to incomming connections
    if (listen(socket_listen, MAX_PEERS) < 0) {
        perror("listen");
        close(socket_listen);
        return -1;
    }

    return socket_listen;
}

//Accepts a new connection and adds to the master set
void new_connection(int socket_listen, fd_set *master, int *fdmax) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_socket = accept(socket_listen, (struct sockaddr *)&client_addr, &addr_len);

    if (new_socket < 0) {
        perror("accept");
        return;
    }

    //Too many peers
    if (peer_count >= MAX_PEERS) {
        printf("Max peers reached. Rejecting connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        close(new_socket);
        return;
    }

    //Add to master set
    FD_SET(new_socket, master);
    if (new_socket > *fdmax) {
        *fdmax = new_socket;
    }

    printf("New connection from %s:%d\n",inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
}

//Processes the message from the peer
void peer_message(int p_socket, fd_set *master) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    int nbytes = recv(p_socket, buffer, sizeof(buffer), 0);

    if (nbytes <= 0) {
        if (nbytes == 0) {
            printf("Socket %d disconnected\n", p_socket);
        } else {
            perror("recv");
        }
        //If error  we can remove peer 
        remove_peer(p_socket, master);
        close(p_socket);
    } else {
        process_command(p_socket, buffer, nbytes);
    }
}

//Handles the commands for each action case
void process_command(int p_socket, char *buffer, int nbytes) {
    uint8_t command;
    memcpy(&command, buffer, sizeof(uint8_t));

    switch (command) {
        case 0:
            join(p_socket, buffer + 1);
            break;
        case 1:
            publish(p_socket, buffer + 1, nbytes - 1);
            break;
        case 2:
            search(p_socket, buffer + 1);
            break;
        default:
            fprintf(stderr, "Unknown command from socket %d\n", p_socket);
    }
}

//Handles joining of a peer to the server
void join(int p_socket, char *data) {
    uint32_t peer_id;

    //Get the ID
    memcpy(&peer_id, data, sizeof(uint32_t));
    peer_id = ntohl(peer_id);

    if (peer_count >= MAX_PEERS) {
        printf("Max peers reached.\n");
        return;
    }

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    //Get the peers address
    getpeername(p_socket, (struct sockaddr *)&addr, &addr_len);

    //Store peer information
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].socket_descriptor == 0) {
            peers[i].id = peer_id;
            peers[i].socket_descriptor = p_socket;
            peers[i].address = addr; 
            peers[i].file_count = 0;
            printf("TEST] JOIN %u\n", peer_id);
            peer_count++;
            return;
        }
    }
}

//Handles publishing of the peer
void publish(int p_socket, char *data, int nbytes) {
    uint32_t file_count;

    //Get the number of files
    memcpy(&file_count, data, sizeof(uint32_t));
    file_count = ntohl(file_count);

    //Find the peer 
    struct peer_entry *peer = NULL;
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].socket_descriptor == p_socket) {
            peer = &peers[i];
            break;
        }
    }

    if (!peer) {
        fprintf(stderr, "Peer not found for socket %d\n", p_socket);
        return;
    }

    if (file_count > MAX_FILES) {
        fprintf(stderr, "Too many files sent by Peer %u\n", peer->id);
        return;
    }

    //Store the files for the peer
    char *filename_ptr = data + sizeof(uint32_t);
    for (uint32_t i = 0; i < file_count; i++) {
        strncpy(peer->files[peer->file_count], filename_ptr, MAX_FILENAME_LEN - 1);
        peer->files[peer->file_count][MAX_FILENAME_LEN - 1] = '\0';
        peer->file_count++;
        filename_ptr += strlen(filename_ptr) + 1;
    }

    printf("TEST] PUBLISH %u", file_count);
    for (int i = 0; i < peer->file_count; i++) {
        printf(" %s", peer->files[i]);
    }
    printf("\n");
}

//Handles the search
void search(int p_socket, char *data) {
    char filename[MAX_FILENAME_LEN];
    strncpy(filename, data, MAX_FILENAME_LEN - 1);
    filename[MAX_FILENAME_LEN - 1] = '\0';

    struct peer_entry *found_peer = NULL;

    // Search through all peers for the file
    for (int i = 0; i < MAX_PEERS; i++) {
        for (int j = 0; j < peers[i].file_count; j++) {
            if (strcmp(peers[i].files[j], filename) == 0) {
                found_peer = &peers[i];
                break;
            }
        }
        if (found_peer) break;
    }

    //Preparation of the response
    if (found_peer) {
        uint32_t response_id = htonl(found_peer->id);
        uint32_t response_ip = found_peer->address.sin_addr.s_addr;
        uint16_t response_port = found_peer->address.sin_port;

        char response[10];
        memcpy(response, &response_id, sizeof(response_id));
        memcpy(response + 4, &response_ip, sizeof(response_ip));
        memcpy(response + 8, &response_port, sizeof(response_port));

        send(p_socket, response, sizeof(response), 0);

        //Convert IP to print
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &found_peer->address.sin_addr, ip, sizeof(ip));
        printf("TEST] SEARCH %s %u %s:%u\n", filename, found_peer->id, ip, ntohs(found_peer->address.sin_port));
    } else {
        char not_found[10] = {0}; // All zeros indicate not found
        send(p_socket, not_found, sizeof(not_found), 0);
        printf("TEST] SEARCH %s 0 0.0.0.0:0\n", filename);
    }
}

//Remove the Peer
void remove_peer(int p_socket, fd_set *master) {
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].socket_descriptor == p_socket) {
            printf("Removing peer %u\n", peers[i].id);
            peers[i].socket_descriptor = 0;
            memset(peers[i].files, 0, sizeof(peers[i].files));
            peer_count--;
            FD_CLR(p_socket, master);
            return;
        }
    }
}
