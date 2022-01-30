#include "operations.h"
#include "operations.c"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define S 20

int minimum_available_session_id(int fd_clients[]){
    for(int i = 0; i < S; i++){
        if(fd_clients[i] == -1){
            return i;
        }
    }
}

int free_client_id_pipe(char *client_pipes[S], int fd_clients[], int session_id){
    client_pipes[session_id] = "";
    fd_clients[session_id] = -1;
}

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *server_pipe = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", server_pipe);

    /*To do*/

    char *client_pipes[S];
    int fd_serv;
    int fd_clients[S];
    int session_id = -1;
    char buf[BUFSIZ];
    int n;

    for(int i = 0; i < S; i++)
        fd_clients[i] = -1;

    if(!valid_pathname(server_pipe)){
        printf("Invalid pipename. \n");
        return 1;
    }

    unlink(server_pipe);

    //Named pipe used by client processes' to send requests to the server
    if(mkfifo(server_pipe, 0777) < 0) return 1;
    if ((fd_serv = open(server_pipe, O_RDONLY)) < 0) return 1;

    while(1) {
        n = read(fd_serv, buf, sizeof(char));
        if (n == 0) break;

        if(buf[0] == '1'){
            int m = read(fd_serv, buf, (40 * sizeof(char)));
            session_id = minimum_available_session_id(fd_clients);
            for(int i = m; i < 40; i++)
                buf[i] = '\0';
            strcpy(client_pipes[session_id], buf);
            if((fd_clients[session_id] = open(client_pipes[session_id], O_WRONLY)) < 0) return -1;
            n = write(fd_clients[session_id], &session_id, sizeof(int));
        }

        if(buf[0] == '2') {
            n = read(fd_serv, &session_id, sizeof(int));
            int client_pipe = fd_clients[session_id];
            free_client_id_pipe(client_pipes, fd_clients, session_id);
            close(client_pipe);
        }

        if(buf[0] == '3') {
            int flags;
            n = read(fd_serv, &session_id, sizeof(int));
            n = read (fd_serv, buf, 40 * sizeof(char));
            n = read(fd_serv, &flags, sizeof(int));
            n = tfs_open(buf, flags);
            n = write(fd_clients[session_id], &n, sizeof(int));
        }

        if(buf[0] == '4') {
            int fhandle;
            n = read(fd_serv, &session_id, sizeof(int));
            n = read(fd_serv, &fhandle, sizeof(int));
            n = tfs_close(fhandle);
            n = write(fd_clients[session_id], &n, sizeof(int));
        }

        if(buf[0] == '5') {
            int fhandle;
            size_t len;
            n = read(fd_serv, &session_id, sizeof(int));
            n = read(fd_serv, &fhandle, sizeof(int));
            n = read(fd_serv, &len, sizeof(size_t));
            char text[len];
            n = read (fd_serv, text, len * sizeof(char));
            n = tfs_write(fd_serv, text, n);
            n =write(fd_clients[session_id], &n, sizeof(int));
        }

        if(buf[0] == '6'){
            int fhandle;
            size_t len;
            n = read(fd_serv, &session_id, sizeof(int));
            n = read(fd_serv, &fhandle, sizeof(int));
            n = read(fd_serv, &len, sizeof(size_t));
            char text[len];
            n = tfs_read(fhandle, text, len);
            fhandle = write(fd_clients[session_id], &n, sizeof(int));
            if (n > 0)
                n = write(fd_clients[session_id], text, (len * sizeof(char)));
        }

        if(buf[0] == '7'){
            n = read(fd_serv, &session_id, sizeof(int));
            n = tfs_destroy_after_all_closed();
            n = write(fd_clients[session_id], &n, sizeof(int));
        }
    }

    close(fd_serv);
    unlink(server_pipe);
    return 0;
}