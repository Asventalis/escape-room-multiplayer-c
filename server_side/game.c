#include "game.h"
#include "timer.h"
#include "server.h"

// ############## FUNZIONI PER GESTIRE lA CREAZIONE E DISTRUZIONE DI PARTITE ##############
// semplifica la scrittura per specificare le locations adiacenti

// funzione chiamata dal timer di un game quando il tempo è scaduto. Non libera niente perché ci penserà il game dato che sarà terminato. Ciò implica che message deve iniziare con END@
void game_timer_expired(void *arg) {
    Timer_Args *args = (Timer_Args *)arg;
    Game *game = (Game *)args->game_ptr;
    char *message = args->message;
    // trova game_index
    int game_index = -1;
    for(int i = 0; i < MAX_GAMES; i++) {
        if(games[i] == game) {
            game_index = i;
            break;
        }
    }
    if(game_index == -1) {
        return;
    }
    // manda messaggio a tutti i giocatori della partita
    send_info_to_all_players(game_index, -1, message);
}

// funzione che aggiunge un timer a un game che quando scade termina la partita stampando il messaggio message
void add_timer_to_game(Game *game, int time_duration, char *message, char *timer_name) {
    Timer *timer = malloc(sizeof(Timer));
    Timer_Args *args = malloc(sizeof(Timer_Args));
    args->game_ptr = (void *)game;
    args->message = str_malloc(message); // crea una copia del messaggio
    timer->args = args;
    timer->timer_id = timer_add(time_duration, game_timer_expired, args);
    timer->timer_name = str_malloc(timer_name); // crea una copia del nome del timer
    timer->next = game->timers_list;
    game->timers_list = timer;
}

// funzione che rimuove un timer da un game
void remove_timer_from_game(Game *game, char *timer_name) {
    Timer *p = game->timers_list;
    Timer *q = NULL;
    while(p) {
        if(strcmp(p->timer_name, timer_name) == 0) {
            if(q) {
                q->next = p->next;
            } else {
                game->timers_list = p->next;
            }
            free(p->timer_name);
            timer_remove(p->timer_id);
            free(p->args->message);
            free(p->args);
            free(p);
            return;
        }
        q = p;
        p = p->next;
    }
    return;
}

// funzione che ritorna l'id di un timer di un game dato il nome del timer
int get_timer_id(Game *game, char *timer_name) {
    Timer *p = game->timers_list;
    while(p) {
        if(strcmp(p->timer_name, timer_name) == 0) {
            return p->timer_id;
        }
        p = p->next;
    }
    return -1;
}

// funzione che rimuove tutti i timer di un game
void remove_all_timers_from_game(Game *game) {
    Timer *p = game->timers_list;
    while(p) {
        Timer *q = p;
        p = p->next;
        free(q->timer_name);
        timer_remove(q->timer_id);
        free(q->args->message);
        free(q->args);
        free(q);
    }
    game->timers_list = NULL;
}

int set_adjacent_locations(Game *game, int index, const int *adjacent, int adjacent_count) {
    if(index >= game->locations_count) {
        printf("Errore: overflow nel vettore delle locations durante la creazione di locations adiacenti\n");
        return -1;
    }
    
    game->locations[index].adjacent_locations_count = adjacent_count;
    // crea un puntatore a int e lo punta a un vettore di interi di dimensione adjacent_count
    int *tmp = malloc(adjacent_count * sizeof(int));
    for(int i = 0; i < adjacent_count; i++) {
        tmp[i] = adjacent[i];
    }
    game->locations[index].adjacent_locations = tmp; // assegna il puntatore al vettore della struttura
    return 0;
}

// funzione che libera la memoria allocata per una location
void free_location(Location *location) {
    free(location->name);
    free(location->description);
    free(location->adjacent_locations);
}

// funzione che libera la memoria allocata per un object
void free_object(Object *object) {
    free(object->name);
    free(object->description);
}

void free_enigma(Enigma *enigma) {
    free(enigma->question);
    free(enigma->answer);
    if(enigma->type == TIMER) {
        free(enigma->timer_name);
    }
}

// funzione che aggiunge una adjacent location a una location
int add_adjacent_location(Game *game, int location_index, int adjacent_location_index) {
    if(location_index >= game->locations_count || adjacent_location_index >= game->locations_count) {
        return -1;
    }
    int *tmp = malloc((game->locations[location_index].adjacent_locations_count + 1) * sizeof(int));
    for(int i = 0; i < game->locations[location_index].adjacent_locations_count; i++) {
        tmp[i] = game->locations[location_index].adjacent_locations[i];
    }
    tmp[game->locations[location_index].adjacent_locations_count] = adjacent_location_index;
    free(game->locations[location_index].adjacent_locations);
    game->locations[location_index].adjacent_locations = tmp;
    game->locations[location_index].adjacent_locations_count++;
    return 0;
}

// funzione che inizializza un game con uno scenario specifico in memoria dinamica e ritorna un puntatore ad essa
Game *create_game(int scenery) {
    Game *game = malloc(sizeof(Game));
    switch(scenery) {
        case 0: // Appartamento con pavimento che crolla
            game->scenery = 0;
            game->token_count = 5;
            game->obtained_tokens = 0;
            game->timers_list = NULL;
            add_timer_to_game(game, 600, "END@Il tempo è scaduto, la partita è finita", "main"); // aggiungi timer principale
            
            game->locations_count = 7;
            game->start_location = 0;
            game->locations = malloc(game->locations_count * sizeof(Location));

            game->locations[0].name = str_malloc("corridoio");
            game->locations[0].description = str_malloc("Sei in un lungo corridoio."
            "\nIl pavimento sta per crollare, lo senti."
            "\nDevi uscire da questo appartamento prima che sia troppo tardi."
            "\nIn fondo al lungo corridoio c'è una ++finestra++."
            "\nA destra c'è la porta della ++cucina++"
            "\nmentre a sinistra c'è la porta del ++bagno++");
            set_adjacent_locations(game, 0, (int[]){1, 2, 3}, 3); // Da Corridoio si può andare in Cucina Bagno e Finestra

            game->locations[1].name = str_malloc("finestra");
            game->locations[1].description = str_malloc("Sei alla finestra."
            "\nFuori c'è una scala anti-incendio. Potresti scendere da lì. "
            "Il problema è che la finestra è chiusa e ci sono delle grate"
            "\nbloccate da un **lucchetto**. Dovresti trovare la chiave...");
            set_adjacent_locations(game, 1, (int[]){0}, 1); // Da Finestra si può andare in Corridoio
            
            game->locations[2].name = str_malloc("cucina");
            game->locations[2].description = str_malloc("Sei nella cucina."
            "\nSembra tutto un po' abbandonato."
            "\nC'è una sporcizia incredibile, e tante cianfrusaglie"
            "\nsparse nel lavandino. Il ++frigo++ ha qualcosa di strano.");
            set_adjacent_locations(game, 2, (int[]){0, 4}, 2); // Da Cucina si può andare in Corridoio o in Frigo
            
            game->locations[3].name = str_malloc("bagno");
            game->locations[3].description = str_malloc("In bagno c'è un odore terribile."
            "\nIl lavandino è intasato e l'acqua non scorre."
            "\nAccanto al water c'è una mensola con delle riviste sparse.");
            set_adjacent_locations(game, 3, (int[]){0}, 1); // Da Bagno si può andare solo in Corridoio
            
            game->locations[4].name = str_malloc("frigo");
            game->locations[4].description = str_malloc("Il frigo è chiuso."
            "\nC'è una **serratura** a codice a 4 cifre che lo tiene chiuso."
            "\nMi sembra esagerato per un frigo...");
            set_adjacent_locations(game, 4, (int[]){2}, 1); // Da Frigo si può andare solo in Cucina e più avanti anche in Stanza segreta

            game->locations[5].name = str_malloc("stanza_segreta");
            game->locations[5].description = str_malloc("Sei nella stanza segreta."
            "\nÈ molto buio e non c'è niente a parte un vecchio +scrigno+"
            "\ne tanta tanta polvere.");
            set_adjacent_locations(game, 5, (int[]){4, 6}, 2); // Da Stanza segreta si può andare in Frigo e in Scrigno

            game->locations[6].name = str_malloc("scrigno");
            game->locations[6].description = str_malloc("Lo scrigno è chiuso."
            "\nHa tre slot: uno a sinistra, uno al centro e uno a destra."
            "\nAttorno ai tre slot c'è la seguente scritta:"
            "\n7 _ 5 _ 2 _ 6 = 11"
            "\ndove i trattini sono rispettivamente i tre slot."
            "\nChe cosa vorrà mai dire?");
            set_adjacent_locations(game, 6, (int[]){5}, 1); // Da Scrigno si può andare solo in Stanza segreta


            game->max_in_bag = 2; // massimo due oggetti nella borsa, come se fossero due mani
            game->objects_count = 10;
            game->objects = malloc(game->objects_count * sizeof(Object));
            game->objects[0].name = str_malloc("rivista");
            game->objects[0].description = str_malloc("Una rivista della settimana enigmistica un po' malconcia."
            "\nÈ aperta ad una pagina con un cruciverba completato a metà."
            "\nC'è una delle definizioni cerchiate."
            "\nLa definizione è: 'Data della scoperta dell'America'");
            game->objects[0].status = IN_LOCATION;
            game->objects[0].position_index = 3;
            
            game->objects[1].name = str_malloc("cacciavite_a_stella");
            game->objects[1].description = str_malloc("Un cacciavite a stella."
            "\nLa sua punta ha questa forma: +");
            game->objects[1].status = IN_LOCATION;
            game->objects[1].position_index = 0;
            
            game->objects[2].name = str_malloc("cacciavite_a_taglio");
            game->objects[2].description = str_malloc("Un cacciavite a taglio."
            "\nLa sua punta ha questa forma: -");
            game->objects[2].status = BLOCKED; // il cacciavite a taglio è bloccato con enigma
            game->objects[2].position_index = 3;
            
            game->objects[3].name = str_malloc("cacciavite_torx");
            game->objects[3].description = str_malloc("Un cacciavite torx."
            "\nLa sua punta ha questa forma: *");
            game->objects[3].status = BLOCKED; // il cacciavite torx è bloccato con enigma
            game->objects[3].position_index = 2;
            
            game->objects[4].name = str_malloc("chiave");
            game->objects[4].description = str_malloc("Una chiave di metallo."
            "\nÈ molto pesante e sembra molto resistente."
            "\nHa una forma strana, non sembra una chiave normale");
            game->objects[4].status = INVISIBLE;
            game->objects[4].position_index = -1;
            
            game->objects[5].name = str_malloc("serratura"); // quella del frigo
            game->objects[5].description = str_malloc("Una serratura a codice a 4 cifre molto robusta."
            "\nNon sembra essere una serratura per "
            "frigo, ma per qualcosa di molto più importante."
            "\nPosso provare a inserire il codice numerico prendendola");
            game->objects[5].status = BLOCKED; // la serratura è bloccata con enigma
            game->objects[5].position_index = 4;
            
            game->objects[6].name = str_malloc("lucchetto"); // quella della finestra
            game->objects[6].description = str_malloc("Un lucchetto molto robusto."
            "\nServe la chiave per aprirlo");
            game->objects[6].status = BLOCKED; // il lucchetto è bloccato senza enigma
            game->objects[6].position_index = 1;
            
            game->objects[7].name = str_malloc("slot*a*sinistra"); // per lo scrigno
            game->objects[7].description = str_malloc("Lo slot a sinistra dello scrigno");
            game->objects[7].status = BLOCKED; // slot a sinistra bloccato senza enigma
            game->objects[7].position_index = 6;
            
            game->objects[8].name = str_malloc("slot*al*centro"); // per lo scrigno
            game->objects[8].description = str_malloc("Lo slot al centro dello scrigno");
            game->objects[8].status = BLOCKED; // slot al centro bloccato senza enigma
            game->objects[8].position_index = 6;
            
            game->objects[9].name = str_malloc("slot*a*destra"); // per lo scrigno
            game->objects[9].description = str_malloc("Lo slot a destra dello scrigno");
            game->objects[9].status = BLOCKED; // slot a destra bloccato senza enigma
            game->objects[9].position_index = 6;


            

            game->enigma_count = 3;
            game->enigmas = malloc(game->enigma_count * sizeof(Enigma));

            game->enigmas[0].question = str_malloc("Ballo quando il gatto non c'è, sono un ----\nCompleta la frase");
            game->enigmas[0].answer = str_malloc("topo");
            game->enigmas[0].type = TIMER;
            game->enigmas[0].time = 30;
            game->enigmas[0].timer_name = str_malloc("timer topo");

            game->enigmas[1].question = str_malloc("Non mi hai nel sacco, sono il -----\nCompleta la frase");
            game->enigmas[1].answer = str_malloc("gatto");
            game->enigmas[1].type = TIMER;
            game->enigmas[1].time = 30;
            game->enigmas[1].timer_name = str_malloc("timer gatto");

            game->enigmas[2].question = str_malloc("Inserisci il codice numerico a quattro cifre per sbloccare il frigo");
            game->enigmas[2].answer = str_malloc("1492");
            game->enigmas[2].type = TRIES;
            game->enigmas[2].max_tries = 3;
            game->enigmas[2].tries = 0;

            
            game->players_count = 0;
            game->next_player_to_create = 0;
            for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
                game->players[i].name[0] = '\0';
                game->players[i].current_location = -1;
                game->players[i].client_id = -1;
                game->players[i].current_enigma = -1;
            }
            return game;
            break;
        default:
            return NULL;
    }
}

// funzione che distrugge un game liberando la memoria allocata
void free_game(Game *game) {
    // libera la memoria delle locations, name, description e adjacent_locations
    remove_all_timers_from_game(game);
    for(int i = 0; i < game->locations_count; i++) {
        free_location(&game->locations[i]);
    }
    free(game->locations);
    // libera la memoria degli objects, name e description
    for(int i = 0; i < game->objects_count; i++) {
        free_object(&game->objects[i]);
    }
    free(game->objects);
    // libera la memoria degli enigmi, question ed eventuali timer_name
    for(int i = 0; i < game->enigma_count; i++) {
        free_enigma(&game->enigmas[i]);
    }
    free(game->enigmas);
    // la memoria dei players non va liberata perché sono array non allocati dinamicamente
    free(game);
}

// funzione che aggiunge un player a un game e ritorna il player_index. Serve solo a gestire la struttura players (aggiorna players_count e next_player_to_create), ma non inizializza il player
int add_player(Game *game) {
    if(game->players_count == MAX_PLAYERS_PER_GAME) { // controllo se il vettore players è pieno
        return -1;
    }
    int player_index = game->next_player_to_create; // prendo l'indice del prossimo player da creare
    game->players[player_index].current_location = game->start_location; // setto la locazione iniziale del player
    game->players[player_index].current_enigma = -1; // setto l'enigma corrente del player a -1
    game->players_count++; // incremento il contatore dei players
    if(game->players_count == MAX_PLAYERS_PER_GAME) { // se il vettore non è pieno, setto next_player_to_create alla prossima posizione libera
        game->next_player_to_create = MAX_PLAYERS_PER_GAME; 
    } else { // altrimenti lo setto alla prossima posizione libera
        do {
            game->next_player_to_create = (game->next_player_to_create + 1) % MAX_PLAYERS_PER_GAME;
        } while(game->players[game->next_player_to_create].name[0] != '\0');
    }
    return player_index;
}

// funzione che rimuove un player da un game, ovvero mette i valori di default e gestisce la struttura players (aggiorna players_count e next_player_to_create)
void remove_player(Game *game, int player_index) {
    // se l'indice del player è fuori dal range oppure il player non esiste, non fa niente
    if(game == NULL || player_index < 0 || player_index >= MAX_PLAYERS_PER_GAME || game->players[player_index].name[0] == '\0') {
        return;
    }
    int scenery = game->scenery;
    switch(scenery) {
        case 0: {
            // lascia a terra gli oggetti del player
            for(int i = 0; i < game->objects_count; i++) {
                if(game->objects[i].status == IN_BAG && game->objects[i].position_index == player_index) {
                    game->objects[i].status = IN_LOCATION;
                    game->objects[i].position_index = game->players[player_index].current_location;
                }
            }
            // se stava facendo un enigma con timer, interrompe il timer
            if(game->players[player_index].current_enigma != -1 && game->enigmas[game->players[player_index].current_enigma].type == TIMER) {
                remove_timer_from_game(game, game->enigmas[game->players[player_index].current_enigma].timer_name);
            }
            // se il giocatore stava in corridoio
            if(game->players[player_index].current_location == 0) {
                // conta il numero di giocatori in corridoio (incluso il giocatore che sta uscendo)
                int players_in_corridoio = 0;
                for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
                    if(game->players[i].current_location == 0) {
                        players_in_corridoio++;
                    }
                }
                // se c'erano due giocatori in corridoio e il timer del corridoio era attivo, lo interrompe
                if(players_in_corridoio == 2 && timer_remaining(get_timer_id(game, "corridoio")) != -1) {
                    remove_timer_from_game(game, "corridoio");
                    // aggiungi info a tutti i giocatori della partita che il timer è stato interrotto
                    char *info = str_malloc("INFO@Il giocatore %s ha lasciato il corridoio, il pavimento non crollerà", game->players[player_index].name);
                    for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
                        if(i != player_index && game->players[i].name[0] != '\0') {
                            send_to_client(game->players[i].client_id, info);
                        }
                    }
                    free(info);
                }
            }
            break;
        }
        default:
            break;
    }
    game->players[player_index].name[0] = '\0'; // setto il nome del player a stringa vuota
    game->players[player_index].client_id = -1; // setto il socket del player a -1
    game->players[player_index].current_location = -1; // setto la locazione del player a -1
    game->players[player_index].current_enigma = -1; // setto l'enigma corrente del player a -1
    game->players_count--; // decremento il contatore dei players
    if(game->next_player_to_create > player_index) { // se la posizione del prossimo player da creare è maggiore di quella del player appena eliminato
        game->next_player_to_create = player_index; // setto next_player_to_create a player_index
    }
}


// funzione che aggiunge e inizializza un game alla lista dei games in corso e ritorna l'indice del game creato
int add_game_to_list(int scenery) {
    pthread_mutex_lock(&mutex_games_array);
    if(current_games_count == MAX_GAMES) { // controllo se il vettore games è pieno
        pthread_mutex_unlock(&mutex_games_array);
        console_printf("Errore: numero massimo di partite raggiunto");
        return -1;
    }
    int game_index = next_game_to_create; // prendo l'indice del game appena creato
    pthread_mutex_lock(&mutex_games[game_index]);
    games[game_index] = create_game(scenery); // creo il game e lo inserisco nel vettore alla posizione game_index = next_game_to_create
    console_printf("Partita creata");
    console_printf("Game pointer: %p", games[game_index]);
    current_games_count++; // incremento il contatore dei games
    if(current_games_count == MAX_GAMES) { // se il vettore è pieno, setto next_game_to_create a -1
        next_game_to_create = MAX_GAMES;
    } else { // altrimenti lo setto alla prossima posizione libera
        do {
            next_game_to_create = (next_game_to_create + 1) % MAX_GAMES;
        } while(games[next_game_to_create] != NULL);
    }
    pthread_mutex_unlock(&mutex_games[game_index]);
    pthread_mutex_unlock(&mutex_games_array);
    return game_index;
}

// funzione che dealloca e rimuove un game dalla lista dei games in corso, solo se non ci sono giocatori
void remove_game_from_list(int game_index) {
    // se l'indice del game è fuori dal range oppure il game non esiste oppure ci sono giocatori, non fa niente
    if(games[game_index] == NULL || game_index < 0 || game_index >= MAX_GAMES || games[game_index]->players_count != 0) {
        return;
    }
    free_game(games[game_index]); // dealloco la memoria del game
    games[game_index] = NULL; // setto a NULL il puntatore al game

    current_games_count--; // decremento il contatore dei games
    if(next_game_to_create > game_index) { // se la posizione del prossimo game da creare è maggiore di quella del game appena eliminato
        next_game_to_create = game_index; // setto next_game_to_create a game_index
    }
}

// funzione che determina se la stringa è un oggetto o una location su cui fare look
int is_lookable(Game *game, int player_index, char *string, int *is_location, int *index) {
    // controlla se la stringa è una location adiacente al player
    int current_location = game->players[player_index].current_location;
    int adj_loc_count = game->locations[current_location].adjacent_locations_count;
    int *adj_loc = game->locations[current_location].adjacent_locations;
    for(int i = 0; i < adj_loc_count; i++) {
        if(strcmp(string, game->locations[adj_loc[i]].name) == 0) {
            *is_location = 1;
            *index = adj_loc[i];
            return 1;
        }
    }
    // controlla se la stringa è un oggetto presente nella location o nello zaino del player
    for(int i = 0; i < game->objects_count; i++) {
        if( // se è nella location del player oppure nella borsa del player e ha lo stesso nome
            (((game->objects[i].status == IN_LOCATION || game->objects[i].status == BLOCKED) && game->objects[i].position_index == current_location)
            || (game->objects[i].status == IN_BAG && game->objects[i].position_index == player_index))
            && strcmp(string, game->objects[i].name) == 0) {
            *is_location = 0;
            *index = i;
            return 1;
        }
    }
    return 0;
}

int is_taken(Game *game, int player_index, char *string, int *index) {
    for(int i = 0; i < game->objects_count; i++) {
        if(game->objects[i].status == IN_BAG && game->objects[i].position_index == player_index && strcmp(string, game->objects[i].name) == 0) {
            if(index != NULL) {
                *index = i;
            }
            return 1;
        }
    }
    return 0;
}

// funzione che aggiunge la lista degli oggetti presenti nella location index alla stringa answer
void append_objects_in_location(Game *game, int index, char **answer) {
    // aggiungi tutti gli oggetti presenti nella location
    char *objects = str_malloc("\nOggetti presenti:");
    int objects_count = 0;
    for(int i = 0; i < game->objects_count; i++) {
        if((game->objects[i].status == IN_LOCATION || game->objects[i].status == BLOCKED) && game->objects[i].position_index == index) {
            objects_count++;
            // aggiungi attorno al nome dell'oggetto 2 asterischi                       \0   *  \n
            objects = realloc(objects, strlen(objects) + strlen(game->objects[i].name) + 1 + 4 + 1); 
            strcat(objects, "\n**");
            strcat(objects, game->objects[i].name);
            strcat(objects, "**");
        }
    }
    if(objects_count == 0) {
        objects = realloc(objects, strlen(objects) + strlen("\nNessun oggetto presente") + 1);
        strcat(objects, "\nNessun oggetto presente");
    }
    *answer = realloc(*answer, strlen(*answer) + strlen(objects) + 1);
    strcat(*answer, objects);
    free(objects);
}

// funzione che aggiunge la lista delle location adiacenti alla location index alla stringa answer
void append_adjacent_locations(Game *game, int index, char **answer) {
    // aggiungi le location adiacenti
    char *adjacent = str_malloc("\nDa qui puoi andare in questi luoghi:");
    for(int i = 0; i < game->locations[index].adjacent_locations_count; i++) {
        // aggiungi attorno al nome della location 2 +
        adjacent = realloc(adjacent, strlen(adjacent) + strlen(game->locations[game->locations[index].adjacent_locations[i]].name) + 1 + 4 + 1);
        strcat(adjacent, "\n++");
        strcat(adjacent, game->locations[game->locations[index].adjacent_locations[i]].name);
        strcat(adjacent, "++");
    }
    *answer = realloc(*answer, strlen(*answer) + strlen(adjacent) + 1);
    strcat(*answer, adjacent);
    free(adjacent);
}

// funzione che dice se un enigma è in risoluzione di un giocatore
int is_enigma_solving(Game *game, int enigma_index) {
    for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
        if(game->players[i].name[0] != '\0' && game->players[i].current_enigma == enigma_index) {
            return 1;
        }
    }
    return 0;
}

// funzione che gestisce l'azione del player in base al comando ricevuto
void game_action(Game* game, int player_index, char *command, char *arg, char **answer, char **info) {
    int scenery = game->scenery;
    switch(scenery) {
        case 0: { // Appartamento con pavimento che crolla
            // controlla prima se il giocatore sta facendo qualche enigma
            if(game->players[player_index].current_enigma != -1) {
                int enigma_index = game->players[player_index].current_enigma;
                Enigma *enigma = &game->enigmas[enigma_index];
                if(enigma->type == TIMER) {
                    // controlla se la risposta è giusta
                    if(strcmp(command, enigma->answer) == 0) {
                        // interrompi il timer dell'enigma
                        remove_timer_from_game(game, enigma->timer_name);
                        // si disattiva l'enigma
                        game->players[player_index].current_enigma = -1;
                        if(enigma_index == 0) {
                            // il giocatore sblocca il cacciavite a taglio e si aggiunge un token
                            game->objects[2].status = IN_BAG;
                            game->objects[2].position_index = player_index;
                            game->obtained_tokens++;
                            *answer = str_malloc("ANSWER@Hai ottenuto il cacciavite a taglio e un token");
                            *info = str_malloc("INFO@Il giocatore %s ha ottenuto un token", game->players[player_index].name);
                            return;
                        } else if(enigma_index == 1) {
                            // il giocatore sblocca il cacciavite torx e si aggiunge un token
                            game->objects[3].status = IN_BAG;
                            game->objects[3].position_index = player_index;
                            game->obtained_tokens++;
                            *answer = str_malloc("ANSWER@Hai ottenuto il cacciavite torx e un token");
                            *info = str_malloc("INFO@Il giocatore %s ha ottenuto un token", game->players[player_index].name);
                            return;
                        }
                    }
                    // se la risposta è sbagliata, manda un messaggio di errore
                    *answer = str_malloc("ANSWER@Risposta sbagliata, riprova. Rimangono %d secondi", timer_remaining(get_timer_id(game, enigma->timer_name)));
                    return;
                } else if(enigma->type == TRIES) {
                    // controlla se la risposta è giusta
                    if(strcmp(command, "exit") == 0) {
                        // se il giocatore vuole uscire dall'enigma, lo fa
                        game->players[player_index].current_enigma = -1;
                        *answer = str_malloc("ANSWER@Sei uscito dall'enigma");
                        return;
                    } else if(strcmp(command, enigma->answer) == 0) {
                        // in base a quale enigma è, fa cose diverse
                        if(enigma_index == 2) {
                            // si aggiunge la stanza segreta a location adiacente del frigo e si aggiunge un token
                            add_adjacent_location(game, 4, 5);
                            game->obtained_tokens++;
                            // si fa scomparire la serratura
                            game->objects[5].status = INVISIBLE;
                            // si disattiva l'enigma
                            game->players[player_index].current_enigma = -1;
                            // aggiorna la descrizione del frigo
                            char *new_frigde_description = str_malloc("Il frigo è aperto."
                            "\nDentro il frigo si intravede un passaggio per arrivare a una stanza segreta."
                            "\nEcco il perché della serratura a codice...");
                            free(game->locations[4].description);
                            game->locations[4].description = new_frigde_description;
                            *answer = str_malloc("ANSWER@Il codice è giusto."
                            "\nHai scoperto una stanza segreta dietro il frigo e hai ottenuto un token");
                            *info = str_malloc("INFO@Il giocatore %s ha ottenuto un token", game->players[player_index].name);
                            return;
                        }
                    } else {
                        // se la risposta è sbagliata, manda un messaggio di errore
                        enigma->tries++;
                        if(enigma->tries == enigma->max_tries) {
                            // se il numero di tentativi massimi è stato raggiunto, finisce il gioco
                            *answer = str_malloc("END@Hai esaurito i tentativi, non riuscirai a scappare. Hai perso");
                            *info = str_malloc("END@Il giocatore %s ha esaurito i tentativi per aprire il frigo. Avete perso", game->players[player_index].name);
                            return;
                        } else {
                            *answer = str_malloc("ANSWER@Risposta sbagliata, rimangono %d tentativi su %d."
                            "\nRiprova oppure digita exit per uscire", enigma->max_tries - enigma->tries, enigma->max_tries);
                            return;
                        }
                    }
                }
            } else if(strcmp(command, "look") == 0) {
                // se non ci sono argomenti, manda la descrizione della location corrente
                if(arg == NULL) {
                    *answer = str_malloc("ANSWER@");
                    // appendi la description della location
                    char *description = game->locations[game->players[player_index].current_location].description;
                    *answer = realloc(*answer, strlen(*answer) + strlen(description) + 1);
                    strcat(*answer, description);
                    // aggiungi la lista degli oggetti presenti nella location
                    append_objects_in_location(game, game->players[player_index].current_location, answer);
                    // aggiungi la lista delle location adiacenti
                    append_adjacent_locations(game, game->players[player_index].current_location, answer);
                    return;
                }
                // controlla se l'argomento è una location adiacente o un oggetto in location o posseduto
                int is_location;
                int index;
                if(is_lookable(game, player_index, arg, &is_location, &index)) {
                    if(is_location) {
                        // sposta il player nella location e manda la descrizione
                        game->players[player_index].current_location = index;
                        *answer = str_malloc("ANSWER@");
                        // appendi la description della location
                        char *description = game->locations[index].description;
                        *answer = realloc(*answer, strlen(*answer) + strlen(description) + 1);
                        strcat(*answer, description);
                        // aggiungi la lista degli oggetti presenti nella location
                        append_objects_in_location(game, game->players[player_index].current_location, answer);
                        // aggiungi la lista delle location adiacenti
                        append_adjacent_locations(game, game->players[player_index].current_location, answer);
                        // manda info a tutti i giocatori della partita che il giocatore si è spostato
                        *info = str_malloc("INFO@Il giocatore %s si è spostato in %s", game->players[player_index].name, game->locations[index].name);
                        // se ci sono più di 2 giocatori in corridoio, fai partire un timer per il crollo del pavimento
                        int players_in_corridoio = 0;
                        for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
                            if(game->players[i].name[0] != '\0' && game->players[i].current_location == 0) {
                                players_in_corridoio++;
                            }
                        }
                        if(index == 0) {
                            if(players_in_corridoio >= 2) {
                                add_timer_to_game(game, 10, "END@Il pavimento è crollato, siete rimasti intrappolati. Avete perso", "corridoio");
                                // aggiumgi info a tutti i giocatori della partita che il timer è partito
                                free(*info);
                                *info = str_malloc("INFO@Il giocatore %s si è spostato in corridoio e adesso siete troppi,"
                                "\nil pavimento crollerà tra 10 secondi", game->players[player_index].name);
                                // aggiungi al messaggio di risposta che il timer è partito
                                *answer = realloc(*answer, strlen(*answer) + strlen("\nSiete troppi in corridoio. Se non ti sposti entro 10 secondi, il pavimento crollerà") + 1);
                                strcat(*answer, "\nSiete troppi in corridoio."
                                "\nSe non ti sposti entro 10 secondi, il pavimento crollerà");
                            }
                        }
                        // se un giocatore è uscito dal corridoio e c'erano esattamente 2 giocatori, interrompi il timer
                        if(players_in_corridoio < 2 && get_timer_id(game, "corridoio") != -1) {
                            remove_timer_from_game(game, "corridoio");
                            // aggiungi info a tutti i giocatori della partita che il timer è stato interrotto
                            free(*info);
                            *info = str_malloc("INFO@Il giocatore %s ha lasciato il corridoio, il pavimento non crollerà", game->players[player_index].name);
                            // aggiungi al messaggio di risposta che il timer è stato interrotto
                            *answer = realloc(*answer, strlen(*answer) + strlen("\nHai lasciato il corridoio. Il pavimento non crollerà") + 1);
                            strcat(*answer, "\nHai lasciato il corridoio. Il pavimento non crollerà");
                        }   
                        return;
                    }
                    // manda la descrizione dell'oggetto
                    *answer = str_malloc("ANSWER@");
                    // appendi la description dell'oggetto
                    char *description = game->objects[index].description;
                    *answer = realloc(*answer, strlen(*answer) + strlen(description) + 1);
                    strcat(*answer, description);
                    return;
                } else {
                *answer = str_malloc("ANSWER@Errore: comando non riconosciuto");
                return;
                }
            } else if(strcmp(command, "objs") == 0) {
                if(arg != NULL) {
                    *answer = str_malloc("ANSWER@Errore: comando objs non accetta argomenti");
                    return;
                }
                // manda la lista degli oggetti posseduti
                *answer = str_malloc("ANSWER@");
                char *objects = str_malloc("Oggetti posseduti:");
                int objects_count = 0;
                for(int i = 0; i < game->objects_count; i++) {
                    if(game->objects[i].status == IN_BAG && game->objects[i].position_index == player_index) {
                        objects_count++;
                        objects = realloc(objects, strlen(objects) + strlen(game->objects[i].name) + 1 + 1);
                        strcat(objects, "\n");
                        strcat(objects, game->objects[i].name);
                    }
                }
                if(objects_count == 0) {
                    objects = realloc(objects, strlen(objects) + strlen("\nNessun oggetto posseduto") + 1);
                    strcat(objects, "\nNessun oggetto posseduto");
                }
                *answer = realloc(*answer, strlen(*answer) + strlen(objects) + 1);
                strcat(*answer, objects);
                free(objects);
                return;
            } else if(strcmp(command, "take") == 0) {
                // conta gli oggetti nello zaino, se sono più di max_in_bag, manda errore
                int in_bag_count = 0;
                for(int i = 0; i < game->objects_count; i++) {
                    if(game->objects[i].status == IN_BAG && game->objects[i].position_index == player_index) {
                        in_bag_count++;
                    }
                }
                if(in_bag_count >= game->max_in_bag) {
                    *answer = str_malloc("ANSWER@Le due mani che hai sono occupate,"
                    "\nlascia un oggetto prima di prenderne altri");
                    return;
                }
                if(arg == NULL) {
                    *answer = str_malloc("ANSWER@Errore: comando take richiede un argomento");
                    return;
                }
                if(is_taken(game, player_index, arg, NULL)) {
                    *answer = str_malloc("ANSWER@Errore: oggetto già posseduto");
                    return;
                }
                // controlla se l'argomento è un oggetto presente nella location
                int is_location;
                int index;
                if(is_lookable(game, player_index, arg, &is_location, &index)) {
                    if(is_location) {
                        *answer = str_malloc("ANSWER@Errore: comando take non accetta location");
                        return;
                    }
                    // controlla se l'oggetto è bloccato
                    if(game->objects[index].status == BLOCKED) {
                        if(strcmp(game->objects[index].name, "cacciavite_a_taglio") == 0) {
                            // controlla se già qualcuno ha l'enigma 0
                            if(is_enigma_solving(game, 0)) {
                                *answer = str_malloc("ANSWER@Un altro tuo compagno sta cercando di prendere l'oggetto");
                                return;
                            }
                            // assegna l'enigma 0 al player
                            game->players[player_index].current_enigma = 0;
                            *answer = str_malloc("ANSWER@Risolvi l'enigma entro %d secondi per prendere l'oggetto."
                            "\nL'enigma è il seguente:\n%s", game->enigmas[0].time, game->enigmas[0].question);
                            add_timer_to_game(game, game->enigmas[0].time, "END@Il tempo è scaduto, l'enigma è fallito e hai perso", game->enigmas[0].timer_name);
                            return;
                        } else if(strcmp(game->objects[index].name, "serratura") == 0) {
                            // controlla se già qualcuno ha l'enigma 2
                            if(is_enigma_solving(game, 2)) {
                                *answer = str_malloc("ANSWER@Un altro tuo compagno sta cercando di aprire il frigo");
                                return;
                            }
                            // assegna l'enigma 2 al player
                            game->players[player_index].current_enigma = 2;
                            *answer = str_malloc("ANSWER@Sembra che si possano fare massimo %d tentativi per aprire la serratura."
                            "\nRimangono ancora %d tentativi."
                            "\nScrivi exit se vuoi evitare di sprecare tentativi",
                            game->enigmas[2].max_tries, game->enigmas[2].max_tries - game->enigmas[2].tries);
                            return;
                        } else if(strcmp(game->objects[index].name, "cacciavite_torx") == 0) {
                            // controlla se già qualcuno ha l'enigma 1
                            if(is_enigma_solving(game, 1)) {
                                *answer = str_malloc("ANSWER@Un altro tuo compagno sta cercando di prendere l'oggetto");
                                return;
                            }
                            // assegna l'enigma 1 al player
                            game->players[player_index].current_enigma = 1;
                            *answer = str_malloc("ANSWER@Risolvi l'enigma entro %d secondi per prendere l'oggetto."
                            "\nL'enigma è il seguente:\n%s", game->enigmas[1].time, game->enigmas[1].question);
                            add_timer_to_game(game, game->enigmas[1].time, "END@Il tempo è scaduto, l'enigma è fallito e hai perso", game->enigmas[1].timer_name);
                            return;
                        } else {
                            *answer = str_malloc("ANSWER@Questo oggetto è bloccato nel suo posto, non puoi raccoglierlo");
                            return;
                        }
                    }
                    // se sono i cacciaviti infilati nello scrigno, devi fare un po' di cose
                    if(index >= 1 && index <= 3 && strstr(game->objects[index].description, "slot") != NULL) {
                        // metti il cacciavite nello zaino e ripristina le descrizioni
                        game->objects[index].status = IN_BAG;
                        game->objects[index].position_index = player_index;
                        int slot_index;
                        char *old_slot_description;
                        if(strstr(game->objects[index].description, "sinistra") != NULL) {
                            slot_index = 7;
                            old_slot_description = str_malloc("Lo slot a sinistra dello scrigno");
                        } else if(strstr(game->objects[index].description, "centro") != NULL) {
                            slot_index = 8;
                            old_slot_description = str_malloc("Lo slot al centro dello scrigno");
                        } else if(strstr(game->objects[index].description, "destra") != NULL) {
                            slot_index = 9;
                            old_slot_description = str_malloc("Lo slot a destra dello scrigno");
                        }
                        free(game->objects[slot_index].description);
                        game->objects[slot_index].description = old_slot_description;
                        char *old_screwdriver_description;
                        if(index == 1) {
                            old_screwdriver_description = str_malloc("Un cacciavite a stella."
                            "\nLa sua punta ha questa forma: +");
                        } else if(index == 2) {
                            old_screwdriver_description = str_malloc("Un cacciavite a taglio."
                            "\nLa sua punta ha questa forma: -");
                        } else if(index == 3) {
                            old_screwdriver_description = str_malloc("Un cacciavite torx."
                            "\nLa sua punta ha questa forma: *");
                        }
                        free(game->objects[index].description);
                        game->objects[index].description = old_screwdriver_description;
                        // stampa messaggio di successo
                        *answer = str_malloc("ANSWER@Hai estratto il %s dallo %s", game->objects[index].name, game->objects[slot_index].name);
                        return;
                    }
                    // sposta l'oggetto dallo stato IN_LOCATION a IN_BAG
                    game->objects[index].status = IN_BAG;
                    game->objects[index].position_index = player_index;
                    *answer = str_malloc("ANSWER@Hai preso l'oggetto %s", game->objects[index].name);
                    return;
                } else {
                    *answer = str_malloc("ANSWER@Errore: comando non riconosciuto");
                    return;
                }
            } else if(strcmp(command, "use") == 0) {
                if(arg == NULL) {
                    *answer = str_malloc("ANSWER@Errore: comando use richiede almeno un argomento");
                    return;
                }
                int is_2_arg2 = strchr(arg, ' ') != NULL;
                // controlla se l'argomento è un oggetto posseduto
                char *obj1 = strtok(arg, " ");
                char *obj2 = strtok(NULL, "");
                if(!is_2_arg2) {
                    int index;
                    if(!is_taken(game, player_index, obj1, &index)) {
                        *answer = str_malloc("ANSWER@Errore: oggetto non posseduto");
                        return;
                    }
                    // non succede niente
                    *answer = str_malloc("ANSWER@Non succede niente");
                    return;
                }
                // controlla se l'argomento è un oggetto posseduto
                int index1, index2, is_2_location;
                if(!is_taken(game, player_index, obj1, &index1)) {
                    *answer = str_malloc("ANSWER@Errore: oggetto %s non posseduto."
                    "\nIl primo oggetto deve essere posseduto", obj1);
                    return;
                }
                if(!is_lookable(game, player_index, obj2, &is_2_location, &index2)) {
                    *answer = str_malloc("ANSWER@Errore: oggetto %s non presente intorno a te e non posseduto.", obj2);
                    return;
                }
                if(is_2_location) {
                    *answer = str_malloc("ANSWER@Errore: oggetto %s non riconosciuto", obj2);
                    return;
                }
                if(index1 >=1 && index1 <= 3 && index2 >=7 && index2 <=9 && strstr(game->objects[index2].description, "cacciavite") == NULL) {
                    // se un cacciavite è usato su uno slot, metti il cacciavite nello scrigno e modifica la description dello slot
                    game->objects[index1].status = IN_LOCATION;
                    game->objects[index1].position_index = 6;
                    char *new_slot_description = str_malloc("Lo slot contiene il %s", game->objects[index1].name);
                    free(game->objects[index2].description);
                    game->objects[index2].description = new_slot_description;
                    char *new_screwdriver_description = str_malloc("Il cacciavite è dentro lo %s", game->objects[index2].name);
                    free(game->objects[index1].description);
                    game->objects[index1].description = new_screwdriver_description;
                    *answer = str_malloc("ANSWER@Hai usato il %s sullo %s."
                    "\nÈ entrato come una chiave."
                    "\nPer estrarre il cacciavite dallo slot, raccoglilo con take.", game->objects[index1].name, game->objects[index2].name);
                    // Controllare se i cacciavite sono messi bene e aprire lo scrigno
                    if(strcmp(game->objects[7].description, "Lo slot contiene il cacciavite_a_stella") == 0 &&
                    strcmp(game->objects[8].description, "Lo slot contiene il cacciavite_torx") == 0 &&
                    strcmp(game->objects[9].description, "Lo slot contiene il cacciavite_a_taglio") == 0) {
                        // fai scomparire gli slot e i cacciaviti
                        game->objects[7].status = INVISIBLE;
                        game->objects[8].status = INVISIBLE;
                        game->objects[9].status = INVISIBLE;
                        game->objects[1].status = INVISIBLE;
                        game->objects[2].status = INVISIBLE;
                        game->objects[3].status = INVISIBLE;
                        // fai apparire la chiave
                        game->objects[4].status = IN_LOCATION;
                        game->objects[4].position_index = 6;
                        // cambia la descrizione dello scrigno
                        char *new_chest_description = str_malloc("Lo scrigno è aperto");
                        free(game->objects[6].description);
                        game->objects[6].description = new_chest_description;
                        // aggiungi un token e manda risposta e info
                        game->obtained_tokens++;
                        *answer = str_malloc("ANSWER@Hai usato i cacciavite nel modo giusto."
                        "\nLo scrigno si è aperto e hai ottenuto un token");
                        *info = str_malloc("INFO@Il giocatore %s ha aperto lo scrigno e ha ottenuto un token", game->players[player_index].name);
                        return;
                    }
                    return;
                } else if(index1 == 4 && index2 == 6) {
                    // se la chiave è usata sul lucchetto, vincono
                    game->obtained_tokens++;

                    *answer = str_malloc("END@Hai aperto il lucchetto e sei riuscito"
                    "\na scappare ottenendo l'ultimo token. Hai vinto");
                    *info = str_malloc("END@Il giocatore %s ha aperto il lucchetto ottenendo"
                    "\nl'ultimo token e siete riusciti a scappare. Avete vinto", game->players[player_index].name);
                    return;
                } else {
                    *answer = str_malloc("ANSWER@Non è successo niente");
                    return;
                }                
            } else if(strcmp(command, "drop") == 0) {
                if(arg == NULL) {
                    *answer = str_malloc("ANSWER@Errore: comando drop richiede un argomento");
                    return;
                }
                if(!is_taken(game, player_index, arg, NULL)) {
                    *answer = str_malloc("ANSWER@Errore: oggetto non posseduto");
                    return;
                }
                int index;
                if(is_taken(game, player_index, arg, &index)) {
                    // sposta l'oggetto dallo stato IN_BAG a IN_LOCATION
                    game->objects[index].status = IN_LOCATION;
                    game->objects[index].position_index = game->players[player_index].current_location;
                    *answer = str_malloc("ANSWER@Hai lasciato l'oggetto %s", game->objects[index].name);
                    return;
                } else {
                    *answer = str_malloc("ANSWER@Errore: comando non riconosciuto");
                    return;
                }
            } else if(strcmp(command, "chat") == 0) {
                if(arg == NULL) {
                    *answer = str_malloc("ANSWER@Errore: comando chat richiede un argomento");
                    return;
                }
                *answer = str_malloc("ANSWER@Hai mandato il messaggio agli altri giocatori");
                *info = str_malloc("INFO@%s: %s", game->players[player_index].name, arg);
                return;
            } else {
                *answer = str_malloc("ANSWER@Errore: comando non riconosciuto");
                return;
            }
            break;
        }
    }
    *answer = NULL;
    *info = NULL;
}
