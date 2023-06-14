#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

char* toLower(char* s)
{
    for (char* p = s; *p; p++) *p = tolower(*p);
    return s;
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        printf("Usage: %s <type> <value>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* type  = argv[1];
    char* value = argv[2];

    if (strcmp(type, "frequency") == 0) {
        // check correct value
        if (atoi(value) < 0 || atoi(value) > 20) {
            printf("Value must be between 0 and 20.\n");
            exit(EXIT_FAILURE);
        }

        int fs_file =
            open("/sys/class/mini_project/supervisor/frequency", O_RDWR);
        if (fs_file < 0) {
            perror("Erreur lors de l'ouverture du fichier frequency");
            exit(EXIT_FAILURE);
        }
        write(fs_file, value, strlen(value));
        close(fs_file);
    } else if (strcmp(type, "mode") == 0) {
        // check correct value
        if (strcmp(toLower(value), "auto") == 0) {
            value = "1";
        } else if (strcmp(toLower(value), "manual") == 0) {
            value = "0";
        } else if (atoi(value) == 0 || atoi(value) == 1) {
            // value is already correct
        } else {
            printf("invalid mode.\n");
            exit(EXIT_FAILURE);
        }

        int mode_file = open("/sys/class/mini_project/supervisor/mode", O_RDWR);
        if (mode_file < 0) {
            perror("Erreur lors de l'ouverture du fichier mode");
            exit(EXIT_FAILURE);
        }
        write(mode_file, value, strlen(value));
        close(mode_file);
    } else {
        printf("Type de donnée invalide.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
