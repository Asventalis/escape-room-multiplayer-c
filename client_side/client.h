#ifndef CLIENT_H
#define CLIENT_H

#include "../shared/console.h"



// ############## VARIABILI GLOBALI ##############
// Strutture per la sincronizzazione tra input_thread e main_thread
pthread_t main_thread; // no mutex per questa variabile, perché viene inizializzata nel main_thread prima di creare altri thread, e poi non viene più modificata
pthread_mutex_t mutex_answer_arrived = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_answer_arrived = PTHREAD_COND_INITIALIZER;
int answer_arrived = 0;



// Funzione per il thread che gestisce l'input del client
void *input_thread_func(void *client_socket_ptr);

// Funzione per inizializzare il client
void client_init();



#endif // CLIENT_H