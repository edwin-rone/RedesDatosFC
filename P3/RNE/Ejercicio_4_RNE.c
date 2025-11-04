#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 7006
#define BUFFER_SIZE 1024
#define MAX_BUFFER 2048

void connect_and_send(const char *server_ip, int server_port, int target_port, int shift, const char *file_content) {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[MAX_BUFFER] = {0};
    char message_to_send[MAX_BUFFER];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Error en la creación del socket para el puerto %d \n", server_port);
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("\nDirección IP inválida o no soportada \n");
        close(sock);
        return;
    }

    printf("[Conectando al puerto %d] Intentando...\n", server_port);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[Conectando al puerto %d] Conexión fallida.\n", server_port);
        close(sock);
        return;
    }

    snprintf(message_to_send, sizeof(message_to_send), "%d %d %s", target_port, shift, file_content);
    send(sock, message_to_send, strlen(message_to_send), 0);

    int bytes_received = recv(sock, buffer, MAX_BUFFER - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("[Respuesta del Servidor %d]: %s\n", server_port, buffer);
    } else {
        printf("[Respuesta del Servidor %d]: No se recibió respuesta o la conexión se cerró.\n", server_port);
    }

    close(sock);
}

int main(int argc, char *argv[]) {
    // Uso: ./clientMulti <IP> <puerto1> <puerto2> ... <archivo1> <archivo2> ... <desplazamiento>
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <IP> <puertos...> <archivos...> <desplazamiento>\n", argv[0]);
        fprintf(stderr, "Nota: Debe haber el mismo número de puertos que de archivos.\n");
        return 1;
    }

    char *server_ip = argv[1];
    int shift = atoi(argv[argc - 1]); // El desplazamiento es el último argumento

    // Calculamos cuántos pares de puerto/archivo hay
    int num_pairs = (argc - 3) / 2;
    if ((argc - 3) % 2 != 0) {
        fprintf(stderr, "Error: El número de puertos no coincide con el número de archivos.\n");
        return 1;
    }

    for (int i = 0; i < num_pairs; i++) {
        int target_port = atoi(argv[2 + i]);
        char *filename = argv[2 + num_pairs + i];

        printf("\n--- Procesando archivo '%s' para puerto %d ---\n", filename, target_port);

        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
            perror("Error al abrir archivo");
            continue; // Saltar al siguiente par
        }
        char file_content[MAX_BUFFER];
        size_t file_size = fread(file_content, 1, MAX_BUFFER - 1, fp);
        file_content[file_size] = '\0';
        fclose(fp);

        connect_and_send(server_ip, target_port, target_port, shift, file_content);
    }

    return 0;
}
