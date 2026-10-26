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
#include <crypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/file.h>
#include <fcntl.h>
#include <syslog.h>
#include <linux/limits.h>

#define PORT 4242
#define BACKLOG 3
#define MAX_CLIENTS 3
#define LOCK_FILE_PATH "/var/lock/ft_shield.lock"
#define BUFFER_SIZE 1024
#define FT_SHIELD_SERVICE_PATH "/etc/systemd/system/ft_shield.service"

int daemonize(void);
