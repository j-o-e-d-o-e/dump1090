#include <string.h>
#include "utils.h"

void trim(char *dest, char *src) {
    unsigned char index = 0, leading_trimmed = 0;
    for (size_t i = 0; i < strlen(src); i++) {
        const char *c = &src[i];
        if (*c != ' ') {
            leading_trimmed = 1;
            dest[index++] = *c;
        } else if (leading_trimmed) {
            dest[index] = '\0';
            break;
        }
    }
}
