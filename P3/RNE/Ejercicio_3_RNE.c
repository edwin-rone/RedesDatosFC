#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>    // Para las variables de error como errno y EINTR
#include <sys/wait.h> // Para la gestión de procesos hijo

#define PORT 7006
#define BUFFER_SIZE 1024
#define MAX_BUFFER 2048

// Función para descifrar un texto usando el cifrado César inverso
void decryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        }
    }
}

// Función para CIFRAR un texto usando el cifrado César
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    if (shift < 0) shift += 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
    }
}

// Función para ejecutar "ip addr show" y guardar la salida en un archivo
void saveNetworkInfo(const char *outputFile) {
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[512];

    fpCommand = popen("ip addr show", "r");
    if (fpCommand == NULL) {
        perror("Error ejecutando popen");
        return;
    }

    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL) {
        perror("[-] Error al abrir el archivo de salida");
        pclose(fpCommand);
        return;
    }

    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
        fputs(buffer, fpOutput);
    }

    fclose(fpOutput);
    pclose(fpCommand);
}

// Función para obtener información del sistema y guardarla en un archivo
void saveSystemInfo(const char *outputFile) {
    // Abre el archivo en modo escritura. Si existe, lo sobrescribe.
    FILE *fp = fopen(outputFile, "w");
    if (fp == NULL) {
        perror("No se pudo crear el archivo sysinfo.txt");
        return;
    }

    printf("[+] Recopilando informacion del sistema...\n");

    // Lista de comandos a ejecutar con sus descripciones
    const char *commands[][2] = {
        {"OS y Kernel", "uname -a"},
        {"Distribucion", "cat /etc/os-release"},
        {"IPs", "ip a"},
        {"CPU Info", "lscpu"},
        {"Memoria", "free -h"},
        {"Disco", "df -h"},
        {"Usuarios Conectados", "who"},
        {"Uptime", "uptime"},
        {"Procesos Activos", "ps aux"},
        {"Directorios Montados", "mount"}
    };
    int num_commands = sizeof(commands) / sizeof(commands[0]);

    // Itera sobre cada comando, lo ejecuta y guarda su salida
    for (int i = 0; i < num_commands; i++) {
        // Escribe un título para la sección
        fprintf(fp, "\n\n==================== %s ====================\n\n", commands[i][0]);
        
        // Prepara el comando para popen, redirigiendo el error estándar a la salida estándar
        char command_buffer[256];
        snprintf(command_buffer, sizeof(command_buffer), "%s 2>&1", commands[i][1]);

        // Ejecuta el comando usando popen
        FILE *cmd_pipe = popen(command_buffer, "r");
        if (cmd_pipe == NULL) {
            fprintf(fp, "Error al ejecutar el comando: %s\n", commands[i][1]);
            continue;
        }

        // Lee la salida del comando y la escribe en el archivo
        char line[1024];
        while (fgets(line, sizeof(line), cmd_pipe) != NULL) {
            fputs(line, fp);
        }
        pclose(cmd_pipe);
    }
    
    // Cierra el archivo
    fclose(fp);
    printf("[+] Informacion guardada en %s\n", outputFile);
}

// Función para enviar un archivo a través de un socket
void sendFile(const char *filename, int sockfd) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] No se pudo abrir el archivo para enviar");
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, bytes, 0) == -1) {
            perror("[-] Error al enviar el archivo");
            break;
        }
    }
    fclose(fp);
}

// Función para convertir una cadena a minúsculas
void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

// Función para eliminar espacios al inicio y al final de una cadena
void trim(char *str) {
    char *start = str;
    while (isspace((unsigned char)*start)) start++;

    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

// Función para verificar si una clave está en el archivo cipherworlds.txt
bool isOnFile(const char *bufferOriginal) {
    FILE *fp;
    char line[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    bool foundWorld = false;

    strncpy(buffer, bufferOriginal, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';
    
    trim(buffer);
    toLowerCase(buffer);

    fp = fopen("cipherworlds.txt", "r");
    if (fp == NULL) {
        printf("[-] Error abriendo cipherworlds.txt!\n");
        return false;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        trim(line);
        toLowerCase(line);
        if (strcmp(line, buffer) == 0) {
            foundWorld = true;
            break;
        }
    }

    fclose(fp);
    return foundWorld;
}

int main(int argc, char *argv[]) {
    int server_ports[] = {49200, 49201, 49202};
    int num_ports = sizeof(server_ports) / sizeof(server_ports[0]);
    int master_sockets[num_ports];

    // --- Crear un socket de escucha para cada puerto ---
    for (int i = 0; i < num_ports; i++) {
        master_sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (master_sockets[i] < 0) {
            perror("Error al crear socket maestro");
            return 1;
        }
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(server_ports[i]);

        if (bind(master_sockets[i], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error en bind");
            return 1;
        }
        if (listen(master_sockets[i], 5) < 0) {
            perror("Error en listen");
            return 1;
        }
        printf("[Servidor] Escuchando en el puerto %d\n", server_ports[i]);
    }

    printf("[Servidor] Listo para aceptar conexiones en todos los puertos.\n");

    // --- Bucle infinito para aceptar clientes ---
    while(1) {
        // Usamos select() para monitorear todos los sockets a la vez
        fd_set readfds;
        int max_sd = 0;
        FD_ZERO(&readfds);

        for (int i = 0; i < num_ports; i++) {
            FD_SET(master_sockets[i], &readfds);
            if (master_sockets[i] > max_sd) {
                max_sd = master_sockets[i];
            }
        }
        
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno != EINTR)) {
            printf("Error en select");
        }

        // Si hay actividad en algún socket, es una nueva conexión
        for (int i = 0; i < num_ports; i++) {
            if (FD_ISSET(master_sockets[i], &readfds)) {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int client_sock = accept(master_sockets[i], (struct sockaddr *)&client_addr, &client_addr_len);

                if (client_sock < 0) {
                    perror("Error en accept");
                    continue;
                }

                // --- Crear un proceso hijo para manejar a este cliente ---
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Error en fork");
                } else if (pid == 0) { // --- Este es el código del proceso HIJO ---
                    // El hijo no necesita los sockets maestros, los cierra
                    for(int j=0; j < num_ports; j++) close(master_sockets[j]);
                    
                    // Lógica de procesamiento
                    char buffer[MAX_BUFFER];
                    int bytes_received = recv(client_sock, buffer, MAX_BUFFER - 1, 0);
                    if (bytes_received > 0) {
                        buffer[bytes_received] = '\0';
                        int target_port, shift;
                        char *file_content;

                        sscanf(buffer, "%d %d", &target_port, &shift);
                        char *first_space = strchr(buffer, ' ');
                        if (first_space != NULL) {
                            char *second_space = strchr(first_space + 1, ' ');
                            if (second_space != NULL) {
                                file_content = second_space + 1;
                                if (target_port == server_ports[i]) {
                                    encryptCaesar(file_content, shift);
                                    send(client_sock, "File received and encrypted.", strlen("File received and encrypted."), 0);
                                    printf("[Puerto %d] Archivo recibido y cifrado.\n", server_ports[i]);
                                } else {
                                    send(client_sock, "REJECTED", strlen("REJECTED"), 0);
                                    printf("[Puerto %d] Petición rechazada para puerto %d.\n", server_ports[i], target_port);
                                }
                            }
                        }
                    }
                    // El hijo ha terminado su trabajo, cierra la conexión y termina
                    close(client_sock);
                    exit(0);
                } else { // --- Este es el código del proceso PADRE ---
                    // El padre no necesita el socket del cliente, lo cierra y sigue esperando
                    close(client_sock);
                }
            }
        }
    }
    return 0;
}
