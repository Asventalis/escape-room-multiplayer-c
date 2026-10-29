#include "timer.h"

// Variabili globali:
Timer_Struct* timer_list; // Lista degli oggetti in attesa del timer
int next_timer_id = 1; // Prossimo id per oggetti timer
pthread_mutex_t mutex_timer; // Mutex di accesso alla lista timer



// Funzione principale del thread di gestione del timer
// Ogni secondo riduce di 1 il conteggio del primo timer sulla lista
// Quando arriva a zero chiama la funzione ad esso legata
void* timer_thread_func(void* p){
    while (1) {
        // Attendo 1 secondo
        sleep(1);
        pthread_mutex_lock(&mutex_stop_listener_and_timer);
        if(stop_listener_and_timer) {
            pthread_mutex_unlock(&mutex_stop_listener_and_timer);
            break;
        }
        pthread_mutex_unlock(&mutex_stop_listener_and_timer);

        // Impedisco che mentre lavoro qualcuno alteri la lista del timer o rimuova un utente
        // (se un utente viene rimosso e chiamo la sua funzione si rompe tutto)
        pthread_mutex_lock(&mutex_timer);
        // Se ci sono timer attivi:
        if (timer_list){
            // Riduco il tempo rimanente al primo in lista (per come sono aggiunti, equivale a riudrlo a tutti)
            timer_list->remaining--;

            // Se il timer arriva a zero lo rimuove e chiama la funzione che contiene
            // While: nel caso più timer arrivino a zero contemporaneamente
            while(timer_list && !timer_list->remaining){
                Timer_Struct* q = timer_list;
                void (*f)(void*) = q->func;
                void* arg = q->arg;

                // Prima di chiamare, e prendere il mutex sull'utente, lascio il mutex sul timer così evito il deadlock
                // La lista verrà lasciata in uno stato consistente anche se succede qualcosa nel mentre
                // L'unico problema sarebbe l'eliminazione dell'utente, bloccata da mutex_user_delete
                pthread_mutex_unlock(&mutex_timer);
                f(arg);
                pthread_mutex_lock(&mutex_timer);

                // Se non è stato eliminato il timer mentre non avevo il lock lo elimino io
                if (timer_list == q){ 
                    timer_list = q->next;
                    free(q);
                }
            }
        }
        pthread_mutex_unlock(&mutex_timer);
    }
    while(timer_list){
        Timer_Struct* q = timer_list;
        timer_list = q->next;
        free(q->arg);
        free(q);
    }
    pthread_exit(NULL);
}



// Aggiunge un timer
int timer_add(int time, void (*f)(void*), void* arg){
    pthread_mutex_lock(&mutex_timer);

    // Creo l'oggetto timer
    Timer_Struct *obj = malloc(sizeof(Timer_Struct));

    obj->id = next_timer_id;
    next_timer_id++;
    obj->next = NULL;
    obj->func = f;
    obj->arg = arg;

    // Scorro la lista (ordinata secondo il tempo rimanente) e inserisco nel punto opportuno
    // Mentre scorro aggiorno time: ogni oggetto contiene il tempo in più rispetto al precedente, non quello totale
    if (timer_list) {
        // Se la lista ha almeno un elemento:
        if (timer_list->remaining >= time) {
            // Caso 1: devo inserire in testa
            obj->remaining = time;
            timer_list->remaining -= time;
            obj->next = timer_list;
            timer_list = obj;
        } else {
            // Devo scorrere la lista:
            Timer_Struct* p = timer_list;
            Timer_Struct* q = NULL;

            while (p->remaining < time && p->next) {
                time -= p->remaining;
                q = p;
                p = p->next;
            }

            if (!p->next){
                // Se sono uscito dal loop perché non ho più elementi dopo
                if (p->remaining >= time) {
                    // Caso 2: inserisco al penultimo posto
                    p->remaining -= time;
                    obj->remaining = time;
                    
                    q->next = obj;
                    obj->next = p;
                } else {
                    // Caso 3: inserisco in coda
                    time -= p->remaining;
                    obj->remaining = time;
                    p->next = obj;
                }
            } else { 
                // Caso 4: inserisco a metà
                p->remaining -= time;
                obj->remaining = time;
                
                q->next = obj;
                obj->next = p;
            }
        }
    } else { 
        // Caso 5: inserisco in lista vuota
        obj->remaining = time;
        timer_list = obj;
    }
    
    int timer_id = obj->id;
    pthread_mutex_unlock(&mutex_timer);
    return timer_id;
}



// Rimuove un timer. Se il timer non esiste non fa nulla
void timer_remove(int id){
    pthread_mutex_lock(&mutex_timer);

    // Se la lista contiene almeno un timer:
    if (timer_list) {
        if (timer_list->id == id) {
            // Se il primo è il timer che mi interessa lo rimuovo
            Timer_Struct* p = timer_list;
            timer_list = p->next;
            // Se ci sono altri timer aggiorno il tempo rimanente
            if (timer_list) {
                timer_list->remaining += p->remaining;
            }

            free(p);
        } else {
            // Se non è il primo scorro la lista
            Timer_Struct* p = timer_list;
            Timer_Struct* q = NULL;

            while (p->id != id && p->next) {
                q = p;
                p = p->next;
            }

            if (p->id == id){
                // Se trovo il timer che mi interessa lo rimuovo
                q->next = p->next;
                // Se ce ne sono di successivi aggiorno il tempo rimanente
                if (p->next) {
                    p->next->remaining += p->remaining;
                }

                free(p);
            }
        }
    }
    pthread_mutex_unlock(&mutex_timer);
}



// Restituisce quanto tempo manca allo scadere di un dato timer
int timer_remaining(int id){
    Timer_Struct* p;
    int time;

    pthread_mutex_lock(&mutex_timer);
    // Controllo la lista dei timer non sia vuota
    if (!timer_list) {
        pthread_mutex_unlock(&mutex_timer);
        return -1;
    }

    // Scorro la lista in cerca del timer specificato calcolando il tempo trascorso
    p = timer_list;
    time = 0;
    while (p->id != id && p->next) {
        time += p->remaining;
        p = p->next;
    }

    // Verifico di aver trovato il timer specificato
    if (p->id != id) {
        pthread_mutex_unlock(&mutex_timer);
        return -1;
    }

    // Aggiorno l'ultima volta il tempo e restituisco il valore trovato
    time += p->remaining;
    pthread_mutex_unlock(&mutex_timer);
    return time;
}