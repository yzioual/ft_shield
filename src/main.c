#include <stdio.h>
#include <stdlib.h>
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

bool has_root_privileges() {
    return (geteuid() == 0);
}

void ft_shield() {
    char *buffer = malloc(sizeof(char) * BUFFER_SIZE);
    if (buffer == NULL) {
        return;
    }
    size_t buffer_size = BUFFER_SIZE;
    getlogin_r(buffer, buffer_size);
    printf("Login: %s\n", buffer);
}

int main(void) {
    umask(0);
    if (!has_root_privileges()) {
        printf("ft_shield needs root privileges\n");
        return (EXIT_FAILURE);
    }

    const char *path = LOCK_FILE_PATH;
    int current_fd = acquire_lock(path);
    if (current_fd != 0) {
        exit(EXIT_FAILURE);
    }


    pid_t pid = fork();
    if (pid > 0) {
        syslog(LOG_NOTICE, "foreground work is done");
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        return (1);
    } else {
        syslog(LOG_NOTICE, "We are so in");
        refresh_pid(current_fd);
        while (true) {
            // here we are gonna have the tcp server listening for three clients
            sleep(1000);
            syslog(LOG_NOTICE, "Work is being done");
        }

        /*
        #include <sys/reboot.h>
        #include <string.h>
        if (reboot(RB_AUTOBOOT) != 0)
            return 1;
            */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
    return (0);
}
