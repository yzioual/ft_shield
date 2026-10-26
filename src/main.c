#include "ft_shield.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int installer_mode(char* self) {
    const char *p = "[Unit]\nDescription=ft_shield service\nAfter=network.target\n[Service]\ntype=forking\nPIDFile=/var/lock/ft_shield.lock\nExecStart=/usr/bin/ft_shield\nRestart=always\nUser=root\n[Install]\nWantedBy=multi-user.target\n";
    (void) p;
    umask(0);
    if (getuid()) {
        printf("ft_shield needs root privileges\n");
        return (EXIT_FAILURE);
    }
    char *buffer = malloc(sizeof(char) * BUFFER_SIZE);
    if (buffer == NULL) {
        return 1;
    }
    size_t buffer_size = BUFFER_SIZE;
    getlogin_r(buffer, buffer_size);
    printf("%s\n", buffer);
    int fd = open(FT_SHIELD_SERVICE_PATH, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return -1;
    }
    write(fd, p, strlen(p));

    int self_file = open(self, O_RDONLY);
    if (self_file < 0) {
        printf("self_file creation has failed\n");
        return -1;
    }

    int copied = open("/bin/ft_shield", O_CREAT | O_WRONLY | O_TRUNC, 0755);
    if (copied < 0) {
        printf("copied creation has failed\n");
        return -1;
    }

    char buf[BUFSIZ+1];
    ssize_t n = 0;
    while ((n = read(self_file, buf, BUFSIZ)) > 0) {
        buf[n] = '\0';
        if ((write(copied, buf, n)) == -1) {
            perror("write");
            exit(1);
        }
    }
    if (n == -1) {
        perror("read");
        exit(1);
    }

    close(self_file);
    close(copied);

    int ret = system("systemctl daemon-reload;systemctl enable ft_shield;systemctl start ft_shield");
    if (ret < 0)
        return -1;

    return 0;
}

int main() {
    char self[PATH_MAX];
    ssize_t len;
    len = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (len == -1) {
        return 2;
    }
    self[len] = '\0';
    if (strcmp(self, "/usr/bin/ft_shield") == 0) {
        return daemonize();
    } else {
        return installer_mode(self);
    }
    return (0);
}
