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
    int n = -1, result;
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

    while (n == -1)
        n = (int)read(fd_client, &result, sizeof (int));
    n = -1;
    while (n == -1)
        n = (int)read(fd_client, &client_session, sizeof (int));

    return result;
}

int tfs_unmount() {
    char op_code = (char) TFS_OP_CODE_UNMOUNT;
    int n = -1, result;

    write(fd_serv, &op_code, sizeof(char));
    write(fd_serv, &client_session, sizeof(int));

    while(n == -1)
        n = read(fd_client, &result, sizeof(int));

    close(fd_serv);
    close(fd_client);
    unlink(client_pipe_name);
    free(client_pipe_name);
    return result;
}

int tfs_open(char const *name, int flags) {
    char op_code = (char) TFS_OP_CODE_OPEN;
    int n = -1, result;

    write(fd_serv, &op_code, sizeof(char));
    write(fd_serv, &client_session, sizeof(int));
    write(fd_serv, name, FILE_NAME_MAX_SIZE * sizeof(char));
    write(fd_serv, &flags, sizeof(int));

    while(n == -1)
        n = read(fd_client, &result, sizeof(int));

    return result;
}

int tfs_close(int fhandle) {
    char op_code = (char) TFS_OP_CODE_CLOSE;
    int n = -1, result;

    write(fd_serv, &op_code, sizeof(char));
    write(fd_serv, &client_session, sizeof(int));
    write(fd_serv, &fhandle, sizeof(int));

    while(n == -1)
        n = read(fd_client, &result, sizeof(int));

    return result;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    char op_code = (char) TFS_OP_CODE_WRITE;
    int n = -1;
    ssize_t result;

    write(fd_serv, &op_code, sizeof(char));
    write(fd_serv, &client_session, sizeof(int));
    write(fd_serv, &fhandle, sizeof(int));
    write(fd_serv, &len, sizeof(size_t));
    write(fd_serv, buffer, len * sizeof(char));

    while(n == -1)
        n = read(fd_client, &result, sizeof(ssize_t));

    return result;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    char op_code = (char) TFS_OP_CODE_READ;
    int n = -1;
    ssize_t result;

    write(fd_serv, &op_code, sizeof(char));
    write(fd_serv, &client_session, sizeof(int));
    write(fd_serv, &fhandle, sizeof(int));
    write(fd_serv, &len, sizeof(size_t));

    while(n == -1)
        n = read(fd_client, &result, sizeof(ssize_t));
    n = -1;
    if(result > 0)
        while (n == -1)
            n = read(fd_client, buffer, result * sizeof(char));
    return result;
}

int tfs_shutdown_after_all_closed() {
    char op_code = (char) TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    int n = -1, result;

    /* TODO: Implement this */

    while(n == -1)
        n = read(fd_client, &result, sizeof(int));

    return result;
}
