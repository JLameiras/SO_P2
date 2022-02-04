#include "operations.h"
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef S
#define S 20
#endif


int minimum_available_session_id();
void free_client_id_pipe(int session_id);
int server_tfs_mount(int session_id);
int server_tfs_unmount(int session_id);
int server_tfs_open(int session_id);
int server_tfs_close(int session_id);
int server_tfs_write(int session_id);
int server_tfs_read(int session_id);
int server_tfs_shutdown_after_all_closed(int session_id);


typedef struct myBuffer {
    char op_code;
    int session_id;
    char name[FILE_NAME_MAX_SIZE];
    char *extra;
    int flags;
    int fhandle;
    size_t len;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} myBuffer;


char *client_pipes[S];
int fd_clients[S], running = 1;
myBuffer buffer[S];


void * worker_thread(void* arg) {
    myBuffer mybuffer = *(myBuffer *)arg;
    int n, session_id = mybuffer.session_id;
    while(running == 1){
        if (pthread_mutex_lock(&buffer[session_id].lock) != 0) continue;

        while(buffer[session_id].op_code == -1)
            pthread_cond_wait(&buffer[session_id].cond, &buffer[session_id].lock);

        if (buffer[session_id].op_code == (char)TFS_OP_CODE_MOUNT)
            server_tfs_mount(session_id);

        if (buffer[session_id].op_code == (char)TFS_OP_CODE_UNMOUNT)
            server_tfs_unmount(session_id);

        if (buffer[session_id].op_code == (char)TFS_OP_CODE_OPEN)
            server_tfs_open(session_id);

        if (buffer[session_id].op_code == (char)TFS_OP_CODE_CLOSE)
            server_tfs_close(session_id);

        if (buffer[session_id].op_code == (char)TFS_OP_CODE_WRITE)
            server_tfs_write(session_id);

        if(buffer[session_id].op_code == (char)TFS_OP_CODE_READ)
            server_tfs_read(session_id);

        if (buffer[session_id].op_code == (char)TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED) {
            server_tfs_shutdown_after_all_closed(session_id);
            break;
        }

        if (pthread_mutex_unlock(&buffer[session_id].lock) != 0) continue;
    }
    return NULL;
}


int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }
    int fd_serv;
    char *server_pipe = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", server_pipe);
    //Named pipe used by client processes to send requests to the server

    unlink(server_pipe);
    if(mkfifo(server_pipe, 0777) < 0) return 1;
    if ((fd_serv = open(server_pipe, O_RDONLY)) < 0) return 1;

    char buf;
    int n, session;
    pthread_t *sessions[S];



    for(int i = 0; i < S; i++) {
        fd_clients[i] = -1;
        buffer[i].op_code = -1; // Initially no tasks for worker threads;
        buffer[i].session_id = i;
        pthread_mutex_init(&buffer[i].lock, NULL);
        pthread_cond_init(&buffer[i].cond, NULL);
        pthread_create(&sessions[i], NULL, worker_thread, &buffer[i]);
    }

    tfs_init();

    while(running == 1) {
        n = read(fd_serv, &buf, sizeof(char));
        if (n == 0)
            break;
        if (buf != (char)TFS_OP_CODE_MOUNT){
            read(fd_serv, &session, sizeof(int));
            pthread_mutex_lock(&buffer[session].lock); // Lock mutex to make changes to buffer
            buffer[session].op_code = buf;
            pthread_cond_signal(&buffer[session].cond);
        }

        if (buf == (char)TFS_OP_CODE_MOUNT){
            if((session = minimum_available_session_id()) == -1) {
                char pipe_name[CLIENT_PIPE_NAME_SIZE];
                read(fd_serv, &pipe_name, (CLIENT_PIPE_NAME_SIZE * sizeof(char)));
                int fd = open(pipe_name, O_WRONLY); // Open client pipe
                write(fd, &session, sizeof(int)); //return -1 to client
                close(fd); // Close client pipe
                continue;
            }
            pthread_mutex_lock(&buffer[session].lock);
            buffer[session].op_code = buf;
            pthread_cond_signal(&buffer[session].cond);
            ssize_t pipe_name_size = read(fd_serv, &buffer[session].name, (CLIENT_PIPE_NAME_SIZE * sizeof(char)));
            for (ssize_t i = pipe_name_size; i < CLIENT_PIPE_NAME_SIZE;
                 i++) // Fill in the rest of the name
                buffer[session].name[i] = '\0';
            pthread_mutex_unlock(&buffer[session].lock);
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
            buffer[session].extra = (char *)malloc(buffer[session].len * sizeof(char));
            buffer[session].len = read (fd_serv, &buffer[session].extra, buffer[session].len * sizeof(char));
        }
        if(buf == (char)TFS_OP_CODE_READ) {
            read(fd_serv, &buffer[session].fhandle, sizeof(int));
            read(fd_serv, &buffer[session].len, sizeof(size_t));
        }
        if(buf == (char)TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED) {}

        if(buf != TFS_OP_CODE_MOUNT) // Unlock previously locked buffer's mutex
            pthread_mutex_unlock(&buffer[session].lock);
    }

    close(fd_serv);
    unlink(server_pipe);
    return 0;
}


/*O servidor mantém apenas uma única tabela de ficheiros abertos, que é usada para
 * servir as chamadas a tfs_open feitas por qualquer cliente.*/


int minimum_available_session_id(){
    for(int i = 0; i < S; i++)
        if(fd_clients[i] == -1) return i;
    return -1;
}


void free_client_id_pipe(int session_id){
    free(client_pipes[session_id]);
    close(fd_clients[session_id]);
    fd_clients[session_id] = -1;
}


int server_tfs_mount(int session_id) {
    int result = 0;
    client_pipes[session_id] = (char *)malloc(CLIENT_PIPE_NAME_SIZE * sizeof(char));
    if(client_pipes[session_id] == NULL) return -1;
    strncpy(client_pipes[session_id], buffer[session_id].name, CLIENT_PIPE_NAME_SIZE);
    if ((fd_clients[session_id] = open(client_pipes[session_id], O_WRONLY)) < 0) {
        result = -1;
        buffer[session_id].op_code = -1;
        return result;
    }
    write(fd_clients[session_id], &result, sizeof(int)); // return 0 to client
    write(fd_clients[session_id], &session_id, sizeof(int)); // return session id to client
    buffer[session_id].op_code = -1;
    return result;
}


int server_tfs_unmount(int session_id) {
    int result = 0;
    write(fd_clients[session_id], &result, sizeof(int));
    free_client_id_pipe(session_id);
    buffer[session_id].op_code = -1;
    return result;
}


int server_tfs_open(int session_id) {
    char buf[FILE_NAME_MAX_SIZE];
    int flags = buffer[session_id].flags, result;
    strncpy(buf, buffer[session_id].name, 40 * sizeof(char));
    result = tfs_open(buf, flags);
    write(fd_clients[session_id], &result, sizeof(int));
    buffer[session_id].op_code = -1;
    return result;
}


int server_tfs_close(int session_id) {
    int result = tfs_close(buffer[session_id].fhandle);
    write(fd_clients[session_id], &result, sizeof(int));
    buffer[session_id].op_code = -1;
    return result;
}


int server_tfs_write(int session_id) {
    int fhandle = buffer[session_id].fhandle;
    size_t len = buffer[session_id].len;
    ssize_t result;
    result = tfs_write(fhandle, buffer[session_id].extra, len * sizeof(char));
    write(fd_clients[session_id], &result, sizeof(ssize_t));
    free(buffer[session_id].extra);
    buffer[session_id].op_code = -1;
    return result < 0 ? -1 : 0;
}


int server_tfs_read(int session_id) {
    int fhandle = buffer[session_id].fhandle;
    size_t len = buffer[session_id].len;
    ssize_t result;
    buffer[session_id].extra = (char *)malloc(len * sizeof(char));
    result = tfs_read(fhandle, buffer[session_id].extra, len);
    write(fd_clients[session_id], &result, sizeof(ssize_t));
    if (result > 0) write(fd_clients[session_id], buffer[session_id].extra, (result * sizeof(char)));
    free(buffer[session_id].extra);
    buffer[session_id].op_code = -1;
    return result < 0 ? -1 : 0;
}


int server_tfs_shutdown_after_all_closed(int session_id) {
    int result;
    result = tfs_destroy_after_all_closed();
    write(fd_clients[session_id], &result, sizeof(int));
    running = 0;
    for(int i = 0; i < S; i++) {
        if (fd_clients[i] != -1 )
            free_client_id_pipe(i);
        pthread_mutex_destroy(&buffer[i].lock);
        pthread_cond_destroy(&buffer[i].cond);
    }
    buffer[session_id].op_code = -1;
    return result;
}