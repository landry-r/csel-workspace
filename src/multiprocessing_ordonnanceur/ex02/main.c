/* Multiprocessing et Ordonnanceur
** ===============================
** Exercice 02:
** CGoups
** ------------------------------------
** Application permettant de valider la capacité des groupes
** de contrôle à limiter l'utilisation de la mémoire.

mount -t tmpfs none /sys/fs/cgroup
mkdir /sys/fs/cgroup/memory
mount -t cgroup -o memory memory /sys/fs/cgroup/memory
mkdir /sys/fs/cgroup/memory/mem
echo $$ > /sys/fs/cgroup/memory/mem/tasks
echo 20M > /sys/fs/cgroup/memory/mem/memory.limit_in_bytes
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 1048576  // 1 Mébibyte (MiB)

int main(void) {
    printf("Hello exercice 02!\n");

    // Nombre de blocs de mémoire à allouer
    int num_blocks = 50;

    // Allouer la mémoire
    char** memory_blocks = (char**)malloc(num_blocks * sizeof(char*));
    if (memory_blocks == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Limiter l'utilisation de la mémoire
    FILE* mem_limit_file = fopen("/sys/fs/cgroup/memory/mem/memory.limit_in_bytes", "w");
    if (mem_limit_file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    fprintf(mem_limit_file, "20M");
    fclose(mem_limit_file);

    // Créer les blocs de mémoire
    int i;
    for (i = 0; i < num_blocks; i++) {
        memory_blocks[i] = (char*)malloc(BLOCK_SIZE * sizeof(char));
        if (memory_blocks[i] == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        // Remplir le bloc avec des 0
        memset(memory_blocks[i], 0, BLOCK_SIZE);
    }

    // Libérer la mémoire
    for (i = 0; i < num_blocks; i++) {
        free(memory_blocks[i]);
    }
    free(memory_blocks);

    printf("Memory allocation and restriction completed successfully.\n");

    return 0;
}
