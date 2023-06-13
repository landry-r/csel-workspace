#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define SERVER_ADDRESS "127.0.0.1"

int send_msg(int socket_server, char* buf)
{
    printf("Envoi de données au serveur : %s\n", buf);
    if (write(socket_server, buf, strlen(buf)) < 0) {
        perror("Erreur lors de l'envoi des données au serveur");
        return -1;
    }

    // Recevoir la réponse du serveur
    char buffer[256];
    int n = read(socket_server, buffer, sizeof(buffer) - 1);
    if (n < 0) {
        perror("Erreur lors de la lecture de la réponse du serveur");
        return -1;
    }

    buffer[n] = '\0';
    printf("Réponse du serveur : %s\n", buffer);

    return 0;
}

int main()
{
    // Étape 1: Créer le socket
    int socket_server = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_server < 0) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // Étape 2: Configurer l'adresse du serveur
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &server_addr.sin_addr) <= 0) {
        perror("Erreur lors de la configuration de l'adresse du serveur");
        exit(EXIT_FAILURE);
    }

    // Étape 3: Établir la connexion avec le serveur
    if (connect(socket_server, (struct sockaddr*)&server_addr, sizeof(server_addr)) <
        0) {
        perror("Erreur lors de la connexion au serveur");
        exit(EXIT_FAILURE);
    }

    printf("Connexion établie avec le serveur.\n");

    // Étape 4: Envoyer des données au serveur
    char* message = "frequency:12";
    send_msg(socket_server, message);

    sleep(1);

    message = "mode:0";
    send_msg(socket_server, message);

    sleep(1);

    message = "frequency:24";
    send_msg(socket_server, message);

    // Étape 6: Fermer la connexion avec le serveur
    close(socket_server);

    return 0;
}