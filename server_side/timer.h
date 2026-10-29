#ifndef TIMER_H
#define TIMER_H

#include "server.h"

// Struttura del timer
typedef struct Timer_Struct {
    int id;                     // Identificatore
    int remaining;              // Tempo rimanente (in più rispetto al precedente in lista)
    struct Timer_Struct* next;         // Prossimo nella lista
    void (*func)(void*);        // Funzione da chiamare allo scadere
    void* arg;                  // Parametro per func
} Timer_Struct;


// Mutex di accesso alla lista timer
extern pthread_mutex_t mutex_timer;

void* timer_thread_func(void*); // Funzione principale del thread di gestione del timer
int timer_add(int, void (*)(void*), void*); // Aggiunge un timer
void timer_remove(int); // Rimuove un timer
int timer_remaining(int); // Restituisce quanto tempo manca allo scadere di un dato timer

#endif // TIMER_H