#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "constants.h"



// ############## FUNZIONI WRAPPER PER LAVORARE CON MEMORIA ALLOCATA DINAMICAMENTE ##############
// funzione per inizializzare una stringa in memoria dinamica
char *str_malloc(const char *format, ...);



// ############## FUNZIONI PER LA COMUNICAZIONE TRA SOCKET ##############
// loop per inviare un messaggio
ssize_t send_loop(int socket, const void *buffer, size_t length);

// loop per ricevere un messaggio
ssize_t recv_loop(int socket, void *buffer, size_t length);

// funzione per inviare un messaggio
int full_send(int socket, const char *message);

// funzione per ricevere un messaggio
char *full_recv(int socket);



// ############## FUNZIONI PER CONTROLLARE I DATI RICEVUTI ##############
// funzione che controlla se il carattere è numerico
int is_num(char c);

// funzione che controlla se il carattere è una lettera
int is_alpha(char c);

// funzione che controlla se la stringa contiene solo caratteri numerici
int str_is_num(char *str);

// funzione che controlla se la stringa contiene solo lettere
int str_is_alpha(char *str);

// funzione che controlla se la stringa contiene solo lettere e numeri
int str_is_alnum(char *str);

// funzione che controlla se la stringa ha una lunghezza tra min e max
int str_is_len(char *str, int min, int max);

// funziona come strtok ma non salta i delimitatori consecutivi
char *my_strtok(char *str, const char *delim);

#endif // UTILS_H