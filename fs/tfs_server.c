#include "operations.h"
#include "operations.c"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define S 20

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
    int fd_client[S];
    int session_id = -1;
    char buf[BUFSIZ];
    int n;

    for(int i = 0; i < S; i++)
        fd_client[i] = -1;

    if(!valid_pathname(server_pipe)){
        printf("Invalid pipename. \n");
        return 1;
    }

    unlink(server_pipe);

    //Named pipe used by client processes' to send requests to the server
    if(mkfifo(server_pipe, 0777) < 0) return 1;
    if ((fd_serv = open(server_pipe, O_RDONLY)) < 0) return 1;

    while(1) {
        n = (int) read (fd_serv, buf, sizeof(char));
        if (n == 0) break;

        if(buf[0] == '1'){
            read (fd_serv, buf, sizeof(char));
            session_id++;
            strcpy(client_pipes[session_id], buf);
        }

       if(buf[0] == '2') {
            read(fd_serv, &session_id, sizeof(int));
            int client_pipe = fd_client[session];
            fd_client[session_id] = -1;
            write(client_pipe, "0", 1);
            close(client_pipe);
        }

        if(buf[0] == '3') {
            int flags;
            read(fd_serv, &session_id, sizeof(int));
            n = read (fd_serv, buf, 40 * sizeof(char));
            read(fd_serv, &flags, sizeof(int));
            n = tfs_open(buf, flags);
            if(n < 0)
                write(fd_client[session_id], -1, sizeof(int));
            else
                write(fd_client[session_id], 0, sizeof(int));
        }

        if(buf[0] == '4') {
            int fhandle;
            read(fd_serv, &session_id, sizeof(int));
            read(fd_serv, &fhandle, sizeof(int));
            n = tfs_close(fhandle);
            if(n < 0)
                write(fd_client[session_id], -1, sizeof(int));
            else
                write(fd_client[session_id], 0, sizeof(int));
        }

        if(buf[0] == '5') {
            int fhandle;
            size_t len;
            read(fd_serv, &session_id, sizeof(int));
            read(fd_serv, &fhandle, sizeof(int));
            read(fd_serv, &len, sizeof(size_t));
            char text[len];
            n = read (fd_serv, text, len * sizeof(char));
            n =write(fd_serv, text, len);
            if(n < 0)
                write(fd_client[session_id], -1, sizeof(int));
            else
                write(fd_client[session_id], 0, sizeof(int));
        }
    }

    close(fd_serv);
    unlink(server_pipe);
    return 0;
}