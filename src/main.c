#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

# define BUFFER_SIZE 1024

int main(void) {
    char *buffer = malloc(sizeof(char) * BUFFER_SIZE);
    if (buffer == NULL) {
        printf("something went wrong with malloc()\n");
        return (1);
    }
    size_t buffer_size = BUFFER_SIZE;
    getlogin_r(buffer, buffer_size);
    printf("%s\n", buffer);
    return (0);
}
