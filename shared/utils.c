#include "utils.h"
#include "constants.h"
#include <arpa/inet.h>



// ############## FUNZIONI WRAPPER PER LAVORARE CON MEMORIA ALLOCATA DINAMICAMENTE ##############
// funzione per inizializzare una stringa in memoria dinamica
char *str_malloc(const char *format, ...) {
    // Usa vasprintf per allocare la memoria necessaria
    va_list args;
    va_start(args, format);
    char *str;
    if(vasprintf(&str, format, args) == -1) {
        va_end(args);
        return NULL;
    }
    va_end(args);
    return str;
}

// ############## FUNZIONI PER LA COMUNICAZIONE TRA SOCKET ##############
// loop per inviare un messaggio
ssize_t send_loop(int socket, const void *buffer, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent = send(socket, (const char *)buffer + total_sent, length - total_sent, 0);
        if (sent <= 0) {
            return -1; // Error occurred
        }
        total_sent += sent;
    }
    return total_sent;
}

// loop per ricevere un messaggio
ssize_t recv_loop(int socket, void *buffer, size_t length) {
    size_t total_received = 0;
    while (total_received < length) {
        ssize_t received = recv(socket, (char *)buffer + total_received, length - total_received, 0);
        // printf("received: %ld\n", received);
        if (received <= 0) {
            return -1; // Error occurred
        }
        total_received += received;
    }
    return total_received;
}

// funzione per inviare un messaggio
int full_send(int socket, const char *message) {
    uint16_t len = strlen(message);
    len = htons(len);
    if (send_loop(socket, &len, sizeof(uint16_t)) == -1) {
        return 0;
    }
    if (send_loop(socket, message, ntohs(len)) == -1) {
        return 0;
    }
    return 1;
}

// funzione per ricevere un messaggio
char *full_recv(int socket) {
    uint16_t len;
    if (recv_loop(socket, &len, sizeof(uint16_t)) == -1) {
        return NULL;
    }
    len = ntohs(len);
    char *message = malloc(len + 1);
    if (recv_loop(socket, message, len) == -1) {
        free(message);
        return NULL;
    }
    message[len] = '\0';
    return message;
}



// ############## FUNZIONI PER CONTROLLARE I DATI RICEVUTI ##############
// funzione che controlla se il carattere è numerico
int is_num(char c) {
    return c >= '0' && c <= '9';
}

// funzione che controlla se il carattere è una lettera
int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// funzione che controlla se la stringa contiene solo caratteri tra '0' e '9'
int str_is_num(char *str) {
    for(int i = 0; i < strlen(str); i++) {
        if(!is_num(str[i])) {
            return 0;
        }
    }
    return 1;
}

// funzione che controlla se la stringa contiene solo lettere
int str_is_alpha(char *str) {
    for(int i = 0; i < strlen(str); i++) {
        if(!is_alpha(str[i])) {
            return 0;
        }
    }
    return 1;
}

// funzione che controlla se la stringa contiene solo lettere e numeri
int str_is_alnum(char *str) {
    for(int i = 0; i < strlen(str); i++) {
        if(!is_alpha(str[i]) && !is_num(str[i])) {
            return 0;
        }
    }
    return 1;
}

// funzione che controlla se la stringa ha una lunghezza tra min e max
int str_is_len(char *str, int min, int max) {
    int len = strlen(str);
    return len >= min && len <= max;
}

// funziona come strtok ma non salta i delimitatori consecutivi
char *my_strtok(char *str, const char *delim) {
    static char *last;
    if (str != NULL) {
        last = str;
    }
    if (*last == '\0') {
        return NULL;
    }
    char *start = last;
    while (*last != '\0' && !strchr(delim, *last)) {
        last++;
    }
    if (*last != '\0') {
        *last = '\0';
        last++;
    }
    return start;
}