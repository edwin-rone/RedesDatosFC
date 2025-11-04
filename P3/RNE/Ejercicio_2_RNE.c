#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 7006
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // --- Ahora son 5 argumentos ---
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <IP Servidor> <Puerto Objetivo> <Desplazamiento> <Archivo>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 192.168.1.157 49202 10 mensaje.txt\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int target_port = atoi(argv[2]);
    int shift = atoi(argv[3]);
    char *filename = argv[4];

    // Leer el archivo que vamos a enviar ---
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Error al abrir el archivo a enviar");
        return 1;
    }
    
    char file_content[BUFFER_SIZE];
    size_t file_size = fread(file_content, 1, BUFFER_SIZE - 1, fp);
    if (file_size == 0) {
        fprintf(stderr, "El archivo está vacío o no se pudo leer.\n");
        fclose(fp);
        return 1;
    }
    file_content[file_size] = '\0';
    fclose(fp);

    // Lista de puertos de los servidores a los que nos conectaremos ---
    int server_ports[] = {49200, 49201, 49202};
    int num_servers = sizeof(server_ports) / sizeof(server_ports[0]);

    printf("Enviando petición a %d servidores. Puerto objetivo: %d\n\n", num_servers, target_port);

    //  Bucle para conectarse a cada servidor ---
    for (int i = 0; i < num_servers; i++) {
        int current_port = server_ports[i];
        int sock;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE] = {0};
        char message_to_send[BUFFER_SIZE];

        // --- Crear y configurar socket (dentro del bucle) ---
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("\n Error en la creación del socket para el puerto %d \n", current_port);
            continue; // Saltar a la siguiente iteración del bucle
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(current_port);

        if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
            printf("\nDirección IP inválida o no soportada \n");
            close(sock);
            continue;
        }

        // --- Conectar al servidor actual ---
        printf("[Conectando al puerto %d] Intentando...\n", current_port);
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("[Conectando al puerto %d] Conexión fallida.\n", current_port);
            close(sock);
            continue;
        }

        // --- Preparar y enviar el mensaje completo ---
        snprintf(message_to_send, sizeof(message_to_send), "%d %d %s", target_port, shift, file_content);
        send(sock, message_to_send, strlen(message_to_send), 0);

        // --- Recibir y mostrar la respuesta ---
        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("[Respuesta del Servidor %d]: %s\n", current_port, buffer);
        } else {
            printf("[Respuesta del Servidor %d]: No se recibió respuesta o la conexión se cerró.\n", current_port);
        }

        close(sock);
    }

    return 0;
}
