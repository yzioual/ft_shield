#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/file.h>
#include <fcntl.h>
#include <syslog.h>

# define BUFFER_SIZE 1024
# define PORT 4242
# define MAX_CLIENTS 3
# define LOCK_FILE_PATH "/var/lock/ft_shield.lock"

int acquire_lock(const char *path)
{
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) return -1;

    // Try to get an exclusive, non-blocking lock
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        // Someone else already holds it
        close(fd);
        return -1;
    }

    // We hold the lock: write our PID for visibility/debugging
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", getpid());
    ftruncate(fd, 0);
    write(fd, buf, len);

    // Keep fd open for the lifetime of the process — closing it
    // (or process exit) releases the lock automatically.
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
    snprintf(buf, sizeof(buf), "%d", pid);
    write(fd, buf, sizeof(buf));
}

#define FT_SHIELD_SERVICE_PATH "/etc/systemd/system/ft_shield.service"

int main() {
    const char *p = "[Unit]\nDescription=My service\nAfter=network.target\n[Service]\nExecStart=/usr/bin/ft_shield\nRestart=always\nUser=root\n[Install]\nWantedBy=multi-user.target\n";

    umask(0);
    if (getuid()) {
        printf("ft_shield needs root privileges\n");
        return (EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == 0) {
        char *buffer = malloc(sizeof(char) * BUFFER_SIZE);
        if (buffer == NULL) {
            return 1;
        }
        size_t buffer_size = BUFFER_SIZE;
        getlogin_r(buffer, buffer_size);
        printf("Login: %s\n", buffer);
        exit(EXIT_SUCCESS);
    } else {
        int ret = system("gcc src/ft_shield.c -o ft_shield;mv ft_shield /usr/bin/");
        if (ret != 0) {
            printf("error\n");
            return 1;
        }

        // write the service script
        int fd = open(FT_SHIELD_SERVICE_PATH, O_CREAT | O_WRONLY, 0644);
        if (fd < 0) {
            return -1;
        }
        write(fd, p, strlen(p));

        system("systemctl daemon-reload");
        system("systemctl enable ft_shield");
        system("systemctl start ft_shield");
    }
    return (0);
}
