#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>


#include "../shared/constants.h"
#include "../shared/console.h"
#include "../shared/utils.h"
#include "client.h"



// Funzione per il thread che gestisce l'input del client
void *input_thread_func(void *client_socket_ptr) {
    // console_printf("(I) Input thread creato");
    int client_socket = *((int*)client_socket_ptr);
    free(client_socket_ptr);
    while(1) {
        char *buf = full_recv(client_socket);
        if(buf == NULL) {
            console_printf("(I) Il server si è disconnesso. Terminazione");
            // manda un segnale al main_thread per terminare il programma e termina il thread
            close(client_socket);
            pthread_kill(main_thread, SIGUSR1);
            pthread_exit(NULL);
        }
        // console_printf("(I) Ricevuto messaggio: %s", buf);
        // Ricopia il messaggio per poter deallocare buf
        int len = strlen(buf);
        char message[len+1];
        strcpy(message, buf);
        free(buf);

        // Spezza la stringa per vedere l'header
        char *header = my_strtok(message, HEADER_DELIMITER);
        char *rest_of_text = my_strtok(NULL, "");
        if(strcmp(header, HEADER_ANSWER) == 0) {
            console_printf(rest_of_text); // stampa messaggio di prompt
            // gestisci sincronizzazione
            pthread_mutex_lock(&mutex_answer_arrived);
            answer_arrived = 1;
            pthread_cond_signal(&cond_answer_arrived);
            pthread_mutex_unlock(&mutex_answer_arrived);
        } else if(strcmp(header, HEADER_INFO) == 0) {
            console_printf(rest_of_text); // stampa messaggio di info
        } else if(strcmp(header, HEADER_END) == 0) {
            console_printf(rest_of_text);
            // manda un segnale al main_thread per terminare il programma e termina il thread
            close(client_socket);
            pthread_kill(main_thread, SIGUSR1);
            pthread_exit(NULL);
        }
         else {
            console_printf("(I) Errore: header non riconosciuto. Header arrivato:\n\n<%s>\n\nTerminazione", header);
            // manda un segnale al main_thread per terminare il programma e termina il thread
            close(client_socket);
            pthread_kill(main_thread, SIGUSR1);
            pthread_exit(NULL);
        }
    }
}

// Funzione che termina il client
void client_terminate() {
    console_restore();
    pthread_mutex_destroy(&mutex_answer_arrived);
    pthread_cond_destroy(&cond_answer_arrived);
}

// signal handler per la terminazione del programma dopo connessione persa dal server
void signal_handler(int sig) {
    if(sig == SIGUSR1) {
        // console_printf("Ricevuto segnale SIGUSR1");
        client_terminate();
        exit(1);
    }
}

// Funzione per inizializzare il client
void client_init() {
    // Inizializza la console
    console_init();

    // Inizializza le strutture per la sincronizzazione tra thread
    main_thread = pthread_self();
    pthread_mutex_init(&mutex_answer_arrived, NULL);
    pthread_cond_init(&cond_answer_arrived, NULL);
    answer_arrived = 0;

    // inizializzo il signal handler
    signal(SIGUSR1, signal_handler);
}

int main(int argc, char *argv[]) {
    // controllo argomenti
    uint16_t port;
    if(argc != 2) {
        fprintf(stderr, "Errore: uso %s <porta>\n", argv[0]);
        exit(1);
    } else  { // controllo se c'è un argomento, ovvero la port
        if(!str_is_num(argv[1])) { // controllo se la port è un numero (solo stringa di cifre)
            fprintf(stderr, "Errore: la port deve essere un numero\n Uso: %s [port]\n", argv[0]);
            exit(1);
        }
        port = atoi(argv[1]); // converto la stringa in intero
        if(port < MIN_PORT || port > MAX_PORT) { // controllo se la port è un numero valido
            fprintf(stderr, "Errore: la port deve essere un numero tra %d e %d\n", MIN_PORT, MAX_PORT);
            exit(1);
        }
    }

    // SOCKET
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket == -1) {
        perror("Errore: creazione socket fallita\n");
        exit(1);
    }

    // CONNECT
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    if(connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        printf("Errore: connessione al server con port %d fallita\n", port);
        perror("Errore\n");
        exit(1);
    }
    // Manda il primo messaggio al server
    if(!full_send(client_socket, FIRST_CONNECTION_MESSAGE)) {
        printf("Errore: invio primo messaggio al server fallito\n");
        exit(1);
    }//else {
    //     printf("Connessione al server riuscita\n");
    // }


    // Avvio thread per la gestione dell'input e del gioco
    int * client_socket_ptr = malloc(sizeof(int));
    *client_socket_ptr = client_socket;
    pthread_t input_thread;
    pthread_create(&input_thread, NULL, &input_thread_func, (void*)client_socket_ptr);
    pthread_detach(input_thread);

    // Inizializzazione client
    client_init();

    while(1) {
        // Aspetta che arrivi la risposta o che il client richieda la terminazione
        pthread_mutex_lock(&mutex_answer_arrived);
        while(!answer_arrived) {
            // console_printf("La risposta non è ancora arrivata");
            pthread_cond_wait(&cond_answer_arrived, &mutex_answer_arrived);
            // console_printf("Controllo se la risposta è arrivata");
        }
        // Se la risposta è arrivata, gestisci la sincronizzazione
        // console_printf("La risposta è arrivata. Rispondi");
        answer_arrived = 0;
        pthread_mutex_unlock(&mutex_answer_arrived);
        // Prendi l'input da tastiera e invialo al server
        char buf[MAX_MSG_LEN];
        console_in(buf);
        full_send(client_socket, buf);
        // Non controllo se l'invio è andato a buon fine, perché se il server si è disconnesso, l'input thread provvederà a terminare il programma
    }

}