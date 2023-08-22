#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char *argv[]) {

    openlog(NULL, 0, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments.");
        exit(1);
    }

    char *writefile = argv[1];
    char *writestr = argv[2];

    FILE *file = fopen(writefile, "w");
    if (!file) {
        syslog(LOG_ERR, "Couldn't open file: %s", writefile);
        exit(1);
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    if (strlen(writestr) != fwrite(writestr, 1, strlen(writestr), file)) {
        syslog(LOG_ERR, "Write to %s failed", writefile);
        exit(1);
    }

    fclose(file);

    return 0;
}
