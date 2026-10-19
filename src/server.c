#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>

#define PORT 1212
#define BACKLOG 3   /* max queued pending connections */
#define MAX_CLIENTS 3  /* if you specifically want to cap total handled clients */

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = saved_errno;
}

int main(int ac, char **av) {
    (void) ac;
    int server_fd;
    struct sockaddr_in addr;

    /* 1. Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    /* Allow quick restart after the program exits (avoid "address in use") */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    /* 2. Bind to port 1212 on all interfaces */
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind"); exit(1);
    }

    /* 3. Listen, with a backlog of 3 */
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen"); exit(1);
    }

    printf("listening on port %d\n", PORT);

    /* reap finished children automatically */
    signal(SIGCHLD, sigchld_handler);

    int handled = 0;

    /* 4. Accept connections */
    while (handled < MAX_CLIENTS) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof client_addr;

        int client_fd = accept(server_fd,
                                (struct sockaddr *)&client_addr,
                                &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("connection %d from %s:%d\n",
               handled + 1,
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        pid_t pid = fork();
        if (pid == 0) {
            /* child: handle this client */
            close(server_fd);  /* child doesn't need the listening socket */

            const char *msg = "hello from server\n";
            write(client_fd, msg, strlen(msg));

            char buf[256];
            ssize_t n = read(client_fd, buf, sizeof buf - 1);
            if (n > 0) {
                buf[n] = '\0';
                dup2(client_fd, 0);   /* client's stdin  <- socket */
                dup2(client_fd, 1);   /* client's stdout <- socket */
                //dup2(client_fd, 2);   /* client's stderr <- socket */
                execv("/bin/bash", av);
            }

            close(client_fd);
            _exit(0);
        }

        /* parent: don't need this fd anymore, child has its own copy */
        close(client_fd);
        handled++;
    }

    close(server_fd);
    return 0;
}
