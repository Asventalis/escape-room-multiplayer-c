#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "../shared/constants.h"
#include "../shared/console.h"
#include "game.h"

// ############## STRUTTURE DATI ##############
enum ServerStatus {
    SERVER_STOPPED,
    SERVER_STARTED
};

enum ClientStatus {
    CLIENT_NOT_LOGGED,
    CLIENT_LOGIN_REQUESTED,
    CLIENT_SIGNUP_REQUESTED,
    CLIENT_LOGGED,
    CLIENT_CHOOSE_SCENERY,
    CLIENT_PLAYING
};

typedef struct {
    pthread_t worker_thread;
    int client_socket;
    char name[MAX_PLAYER_NAME_LEN+1];
    int status;
    int game_index;
    int player_index;
} Client; // struttura per gestire i client al di fuori delle partite


// ############## VARIABILI GLOBALI ##############
// strutture per gestire le varie partite
Game *games[MAX_GAMES];
pthread_mutex_t mutex_games_array; // mutex per operazioni di inserimento e rimozione di partite sull'intervo vettore
int current_games_count; // il conteggio delle partite attive
int next_game_to_create; // è sempre il più piccolo indice libero nel vettore games
pthread_mutex_t mutex_games[MAX_GAMES]; // mutex per operazioni di modifica di una singola partita

// strutture per fermare il listener a richiesta
pthread_t listener_thread;
pthread_t timer_thread;
int stop_listener_and_timer;
pthread_mutex_t mutex_stop_listener_and_timer;

// strutture per gestire i client
Client *clients[MAX_CLIENTS];
pthread_mutex_t mutex_clients_array;
int current_clients_count;
int next_client_to_create;
pthread_mutex_t mutex_clients[MAX_CLIENTS];



// ############## FUNZIONI PER LAVORARE CON IL VETTORE GAMES ##############
// funzione che alloca e inserisce un client nella lista dei client e lo inizializza solo con il socket e gli altri campi a valori di default
int add_client(int client_socket);

// funzione che rimuove e dealloca un client dalla lista dei client
int remove_client(int client_index);

// funzione che cambia lo stato di un client. Se il client non esiste, non fa niente
void change_client_status(int client_id, int new_status);

// funzione che invia un messaggio a un client. Se il client non esiste, non fa niente e ritorna 0. Altrimenti ritorna il valore di full_send
int send_to_client(int client_id, const char *message);

// funzione che rimuove client dalla lista dei clients, eventualmente anche dalla partita e eventualmente rimuove anche la partita. Se qualche struttura è già vuota, non fa niente
void disconnect_client(int client_id);

// funzione per controllare che l'input sia name e password
int is_name_password(char *name, char *password);

// funzione che inizializza tutte le strutture necessarie al server
void server_init();

// funzione che distrugge tutte le strutture del server
void server_delete();

// funzione che avvia il server. Questa funzione deve creare un thread listener che esegue la funzione listener_thread_func
void start_server(uint16_t port);

// funzione che ferma il server. Questa funzione deve settare la variabile stop_listener_and_timer a 1 e aspettare che il listener thread termini
void stop_server();

// funzione che gestisce il thread listener
void *listener_thread_func(void *port_ptr);

// funzione che gestisce il thread worker
void *worker_thread_func(void *client_id_ptr);

// funzione che invia un messaggio di informazioni a tutti i giocatori di una partita tranne player_index. Se player_index non è tra 0 e MAX_PLAYERS_PER_GAME-1, invia a tutti i giocatori della partita
void send_info_to_all_players(int game_index, int player_index, const char *message);

// funzione che costruisce una stringa con tutte le partite disponibili con pattern "room_index room_scenery num_players\n"
char *get_active_games_list();

// funzione che aggiunge un player alla partita games[game_index] e copia i dati del client clients[client_id] nel player
int add_client_to_game(int client_id, int game_index);

// funzione che gestisce la risposta del client quando è in stato CLIENT_NOT_LOGGED
char *handle_client_not_logged(int client_id, char *client_message);

// funzione che gestisce la risposta del client quando è in stato CLIENT_SIGNUP_REQUESTED
char *handle_client_signup_requested(int client_id, char *client_message);

// funzione che gestisce la risposta del client quando è in stato CLIENT_LOGIN_REQUESTED
char *handle_client_login_requested(int client_id, char *client_message);

// funzione che gestisce la risposta del client quando è in stato CLIENT_LOGGED
char *handle_client_logged(int client_id, char *client_message);

// funzione che gestisce la risposta del client quando è in stato CLIENT_CHOOSE_SCENERY
char *handle_client_choose_scenery(int client_id, char *client_message);

// funzione che gestisce la risposta del client quando è in stato CLIENT_PLAYING
char *handle_client_playing(int client_id, char *client_message);

// funzione che elabora il messaggio del client e calcola la risposta
char *elaborate_message(int client_id, char *client_message);

// funzione che stampa tutte le info di un client a una certa tabulazione
void print_client_info(int client_id, int n_spaces);

// funzione che stampa tutte le info di un game a una certa tabulazione
void print_game_info(int game_index, int n_spaces);


#endif // SERVER_H