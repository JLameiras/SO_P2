#include "tecnicofs_client_api.h"
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int client_session = -1; // Is this how we are supposed to store the client's id?
int fd_client, fd_serv;
char *client_pipe_name;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    char op_code = (char) TFS_OP_CODE_MOUNT;
    int n = 0, result;
    // Create client pipe and open it for reading
    unlink(client_pipe_path);
    if(mkfifo(client_pipe_path, 0777) < 0) return -1;
    client_pipe_name = (char *) malloc(CLIENT_PIPE_NAME_SIZE * sizeof(char));
    strncpy(client_pipe_name, client_pipe_path, 40);
    if((fd_serv = open(server_pipe_path, O_WRONLY)) < 0) {
        unlink(client_pipe_path);
        free(client_pipe_name);
        return -1;
    }
    write(fd_serv, &op_code, sizeof(char));
    write(fd_serv, client_pipe_path, CLIENT_PIPE_NAME_SIZE * sizeof(char));
    if((fd_client = open(client_pipe_path, O_RDONLY)) < 0) {
        close(fd_serv);
        unlink(client_pipe_path);
        free(client_pipe_name);
        return -1;
    }
    while (n == 0) {
        n = (int)read(fd_serv, &result, sizeof (int));
        if(n != 0)
            read(fd_serv, &client_session, sizeof(int));
    }
    return result;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
