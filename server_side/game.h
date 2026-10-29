#ifndef GAME_H
#define GAME_H

#include "../shared/constants.h"
#include "../shared/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>



// ############## STRUTTURE DATI ##############
enum OBJECT_POSITION {
    INVISIBLE,
    IN_LOCATION,
    IN_BAG,
    BLOCKED
};

enum EVENTS_SCENERY_0 {
    TIMER_ENDS,
    FLOOR_COLLAPSE
};

enum ENIGMA_TYPE {
    TRIES,
    TIMER,
    TRIES_TIMER
};

typedef struct {
    char *name;
    char *description;
    int *adjacent_locations;
    int adjacent_locations_count;
} Location;

typedef struct {
    char *name;
    char *description;
    int status;
    int position_index;     // Se stato == IN_LOCATION, indice della locazione in cui si trova
} Object;                   // Se stato == IN_BAG, indice del giocatore che lo possiede
                            // Se stato == INVISIBLE, l'oggetto non è presente
                            // Se stato == BLOCKED, indice della locazione

typedef struct {
    char name[MAX_PLAYER_NAME_LEN+1];
    int current_location;
    int client_id;
    int current_enigma;
} Player;

typedef struct {
    char *question;
    char *answer;
    int type;
    // TIPO TRIES
    int tries;
    int max_tries;
    // TIPO TIMER
    char *timer_name;
    int time;
} Enigma;

typedef struct {
    void *game_ptr;
    char *message;
} Timer_Args;

typedef struct Timer {
    Timer_Args *args;
    int timer_id;
    char *timer_name;
    struct Timer *next;
} Timer;

typedef struct {
    int scenery;
    int token_count;
    int obtained_tokens;
    
    int locations_count;
    Location *locations;
    int start_location;

    Timer *timers_list;

    int max_in_bag;
    int objects_count;
    Object *objects;

    int enigma_count;
    Enigma *enigmas;

    Player players[MAX_PLAYERS_PER_GAME];
    int players_count;
    int next_player_to_create;
} Game;

// ############## FUNZIONI PER GESTIRE lA CREAZIONE E DISTRUZIONE DI PARTITE ##############
// funzione che aggiunge un timer a un game che quando scade termina la partita stampando il messaggio message
void add_timer_to_game(Game *game, int time_duration, char *message, char *timer_name);

// funzione che rimuove un timer da un game
void remove_timer_from_game(Game *game, char *timer_name);

// funzione che ritorna l'id di un timer di un game dato il nome del timer
int get_timer_id(Game *game, char *timer_name);

// funzione che rimuove tutti i timer di un game
void remove_all_timers_from_game(Game *game);

// semplifica la scrittura per specificare le locazioni adiacenti
int set_adjacent_locations(Game *game, int index, const int *adjacent, int adjacent_count);

// funzione che libera la memoria allocata per una location
void free_location(Location *location);

// funzione che libera la memoria allocata per un object
void free_object(Object *object);

void free_enigma(Enigma *enigma);

// funzione che inizializza un game con uno scenario specifico in memoria dinamica e ritorna un puntatore ad essa
Game *create_game(int scenery);

// funzione che distrugge un game liberando la memoria allocata
void free_game(Game *game);

// funzione che aggiunge un player a un game e ritorna il player_index
int add_player(Game *game);

// funzione che rimuove un player da un game
void remove_player(Game *game, int player_index);



// ############## FUNZIONI PER GESTIRE LA LISTA DI GAMES ##############
// funzione che aggiunge una partita alla lista dei games in corso e ritorna l'indice del game creato
int add_game_to_list(int scenario);

// funzione che rimuove una partita dalla lista dei games in corso
void remove_game_from_list(int indice);

void terminate_game(Game *game);

// funzione che gestisce l'azione del player in base al comando ricevuto
void game_action(Game* game, int player_index, char *command, char *arg, char **answer, char **info);

#endif // GAME_H