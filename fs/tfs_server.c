#include "operations.h"
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef S
#define S 20
#endif


int minimum_available_session_id(int fd_clients[]);
void free_client_id_pipe(char *client_pipes[S], int fd_clients[], int session_id);
int server_tfs_mount(int fd_serv, int fd_clients[], char *client_pipes[]);
int server_tfs_unmount(int fd_serv, int fd_clients[], char *client_pipes[]);
int server_tfs_open(int fd_serv, int fd_clients[]);
int server_tfs_close(int fd_serv, int fd_clients[]);
int server_tfs_write(int fd_serv, int fd_clients[]);
int server_tfs_read(int fd_serv, int fd_clients[]);
int server_tfs_shutdown_after_all_closed(int fd_serv, int fd_clients[], char *client_pipes[]);


typedef struct myBuffer {
    char op_code;
    int session_id;
    char name[FILE_NAME_MAX_SIZE];
    int flags;
    int fhandle;
    size_t len;
    pthread_mutex_t lock;
} myBuffer;


char *client_pipes[S];
int fd_clients[S];
myBuffer buffer[S];
char *extra_buffer[S];


void * worker_thread(void* arg) {
    myBuffer *buf = arg;

    return NULL;
}


int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *server_pipe = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", server_pipe);

    int fd_serv;
    char buf;
    int n, session;
    pthread_t *sessions[S];

    for(int i = 0; i < S; i++) {
        fd_clients[i] = -1;
        buffer[i].op_code = -1; // Initially no tasks for worker threads;
        buffer[i].session_id = i;
        pthread_mutex_init(&buffer[i].lock, NULL);
        pthread_create(sessions[i], NULL, worker_thread, &buffer[i]);
    }

    unlink(server_pipe);

    //Named pipe used by client processes to send requests to the server
    if(mkfifo(server_pipe, 0777) < 0) return 1;
    if ((fd_serv = open(server_pipe, O_RDONLY)) < 0) return 1;
    tfs_init();

    while(1) {
        n = read(fd_serv, &buf, sizeof(char));
        if (n == 0)
            break;
        read(fd_serv, &session, sizeof(int));
        buffer[session].op_code = buf;
        if (buf != TFS_OP_CODE_MOUNT){
            buffer[session].session_id = session;
        }

        if (buf == (char)TFS_OP_CODE_MOUNT){
            if((session = minimum_available_session_id(fd_clients)) == -1) continue;
            ssize_t pipe_name_size = read(fd_serv, &buffer[session].name, (CLIENT_PIPE_NAME_SIZE * sizeof(char)));
            for (ssize_t i = pipe_name_size; i < CLIENT_PIPE_NAME_SIZE;
                 i++) // Fill in the rest of the name
                buffer[session].name[i] = '\0';
        }

        if (buf == (char)TFS_OP_CODE_UNMOUNT){}

        if (buf == (char)TFS_OP_CODE_OPEN) {
            read(fd_serv, &buffer[session].name, (FILE_NAME_MAX_SIZE * sizeof(char)));
            read(fd_serv, &buffer[session].flags, sizeof(int));
        }
        if (buf == (char)TFS_OP_CODE_CLOSE)
            read(fd_serv, &buffer[session].fhandle, sizeof(int));

        if (buf == (char)TFS_OP_CODE_WRITE){
            read(fd_serv, &buffer[session].fhandle, sizeof(int));
            read(fd_serv, &buffer[session].len, sizeof(size_t));
            extra_buffer[session] = (char *)malloc(buffer[session].len * sizeof(char));
            read (fd_serv, &extra_buffer[session], buffer[session].len * sizeof(char));
        }
        if(buf == (char)TFS_OP_CODE_READ) {
            read(fd_serv, &buffer[session].fhandle, sizeof(int));
            read(fd_serv, &buffer[session].len, sizeof(size_t));
        }
        if(buf == (char)TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED) // Desligar o servidor depois de todos os clientes fecharem os ficheiros
            if(server_tfs_shutdown_after_all_closed(fd_serv, fd_clients, client_pipes) == 0) break;
    }

    close(fd_serv);
    unlink(server_pipe);
    return 0;
}


/*O servidor mantém apenas uma única tabela de ficheiros abertos, que é usada para
 * servir as chamadas a tfs_open feitas por qualquer cliente.*/


int minimum_available_session_id(int fd_clients[]){
    for(int i = 0; i < S; i++)
        if(fd_clients[i] == -1) return i;
    return -1;
}


void free_client_id_pipe(char *client_pipes[S], int fd_clients[], int session_id){
    free(client_pipes[session_id]);
    close(fd_clients[session_id]);
    fd_clients[session_id] = -1;
    pthread_mutex_destroy(&buffer[session_id].mutex);
}


int server_tfs_mount(int session_id) {
    char buf[40];
    int valid_mount = 0;
    valid_mount = session_id == -1 ? -1 : 0;
    if(valid_mount == 0) {
        client_pipes[session_id] = (char *)malloc(CLIENT_PIPE_NAME_SIZE * sizeof(char));
        if(client_pipes[session_id] == NULL) return -1;
        strncpy(client_pipes[session_id], buffer.name, CLIENT_PIPE_NAME_SIZE);
        if ((fd_clients[session_id] = open(client_pipes[session_id], O_WRONLY)) < 0)
            return -1;
        write(fd_clients[session_id], &valid_mount, sizeof(int)); // return 0 to client
        write(fd_clients[session_id], &session_id, sizeof(int)); // return session id to client
        return 0;
    } else {
        // In this case client session isn't created so pipe name is not to be stored
        int fd = open(buf, O_WRONLY);
        write(fd_clients[session_id], &valid_mount, sizeof(int)); //return -1 to client
        close(fd);
        return -1;
    }
}


int server_tfs_unmount(int session_id) {
    free_client_id_pipe(client_pipes, fd_clients, session_id);
    return 0;
}


int server_tfs_open(int session_id) {
    char buf[FILE_NAME_MAX_SIZE];
    int flags, result;
    result = tfs_open(buf, flags);
    write(fd_clients[session_id], &result, sizeof(int));
    return result;
}


int server_tfs_close(int session_id) {
    int result = tfs_close(buffer[session_id].fhandle);
    write(fd_clients[session_id], &result, sizeof(int));
    return result;
}


int server_tfs_write(int session_id) {
    int session_id, fhandle;
    size_t len;
    ssize_t result;
    read(fd_serv, &session_id, sizeof(int));
    read(fd_serv, &fhandle, sizeof(int));
    read(fd_serv, &len, sizeof(size_t));
    char *text = (char *)malloc(len * sizeof(char));
    len = read (fd_serv, text, len * sizeof(char));
    result = tfs_write(fhandle, text, len * sizeof(char));
    write(fd_clients[session_id], &result, sizeof(ssize_t));
    free(text);
    return result < 0 ? -1 : 0;
}


int server_tfs_read(int fd_serv, int fd_clients[]) {
    int session_id, fhandle;
    size_t len;
    ssize_t result;
    read(fd_serv, &session_id, sizeof(int));
    read(fd_serv, &fhandle, sizeof(int));
    read(fd_serv, &len, sizeof(size_t));
    char *text = (char *)malloc(len * sizeof(char));
    result = tfs_read(fhandle, text, len);
    write(fd_clients[session_id], &result, sizeof(ssize_t));
    if (result > 0) write(fd_clients[session_id], text, (result * sizeof(char)));
    free(text);
    return result < 0 ? -1 : 0;
}


int server_tfs_shutdown_after_all_closed(int fd_serv, int fd_clients[], char *client_pipes[]) {
    int session_id, result;
    read(fd_serv, &session_id, sizeof(int));
    result = tfs_destroy_after_all_closed();
    write(fd_clients[session_id], &result, sizeof(int));
    for(int i = 0; i < S; i++) {
        if (fd_clients[i] != -1 )
            free_client_id_pipe(client_pipes, fd_clients, i);
    }
    return result;
}