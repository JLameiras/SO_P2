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
    int session_id = -1;
    int fd_serv, fd_client[S];
    int buf[BUFSIZ];
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
        n = read (fd_serv, buf, sizeof(char));
        if (n == 0) continue;

        if(buf[0] == '1'){
            n = read (fd_serv, buf, (40 * sizeof(char)));
            session_id++;
            strcpy(client_pipes[session_id], buf,(40 * sizeof(char)));


        }
        else if(buf[0] == '2') {
            int session = -1;
            n = read(fd_serv, session, sizeof(int));
            int client_pipe = fd_client[session];
            fd_client[session] = -1;
            close(client_pipe);
        }

        //Handle requests...
        //session_id is created by client when initializing a new session
        //Pedido de sessão
        //session_id++;
    }

    close(fd_serv);
    close(fd_client);
    unlink(server_pipe);
    unlink(client_pipes[session_id]);
    return 0;
}