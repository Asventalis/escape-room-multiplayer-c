#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>

#include "../shared/constants.h"
#include "../shared/utils.h"
#include "../shared/console.h"
#include "server.h"
#include "game.h"
#include "timer.h"



// ############## FUNZIONI PER LAVORARE CON LE STRUTTURE DATI ##############
// funzione che alloca e inserisce un client nella lista dei client e lo inizializza solo con il socket e gli altri campi a valori di default
int add_client(int client_socket) {
    if(current_clients_count == MAX_CLIENTS) { // controllo se il vettore clients è pieno
        console_printf("Errore: numero massimo di client raggiunto");
        return -1;
    }
    int client_index = next_client_to_create; // prendo l'indice del prossimo client da creare
    clients[client_index] = malloc(sizeof(Client));
    clients[client_index]->client_socket = client_socket; // setto il socket del client
    clients[client_index]->name[0] = '\0'; // setto il nome del client a stringa vuota
    clients[client_index]->status = CLIENT_NOT_LOGGED; // setto lo stato del client
    clients[client_index]->game_index = -1; // setto l'indice del game a -1
    clients[client_index]->player_index = -1; // setto l'indice del player a -1

    current_clients_count++; // incremento il contatore dei client
    if(current_clients_count == MAX_CLIENTS) { // se il vettore è pieno, setto next_client_to_create a -1
        next_client_to_create = MAX_CLIENTS;
    } else { // altrimenti lo setto alla prossima posizione libera
        do {
            next_client_to_create = (next_client_to_create + 1) % MAX_CLIENTS;
        } while(clients[next_client_to_create] != NULL);
    }
    return client_index;
}

// funzione che rimuove e dealloca un client dalla lista dei client
int remove_client(int client_index) {
    if(clients[client_index] == NULL || client_index < 0 || client_index >= MAX_CLIENTS) { // controllo input
        console_printf("Errore: si è cercato di rimuovere il client %d ma non esiste", client_index);
        return 0;
    }
    free(clients[client_index]); // dealloco la memoria del client
    clients[client_index] = NULL; // setto a NULL il puntatore al client

    current_clients_count--; // decremento il contatore dei client
    if(next_client_to_create > client_index) { // se la posizione del prossimo client da creare è maggiore di quella del client appena eliminato
        next_client_to_create = client_index; // setto next_client_to_create a client_index
    }
    return 1;
}

// funzione che cambia lo stato di un client. Se il client non esiste, non fa niente
void change_client_status(int client_id, int new_status) {
    pthread_mutex_lock(&mutex_clients[client_id]);
    if(clients[client_id] == NULL) {
        pthread_mutex_unlock(&mutex_clients[client_id]);
        return;
    }
    clients[client_id]->status = new_status;
    pthread_mutex_unlock(&mutex_clients[client_id]);
}

// funzione che invia un messaggio a un client. Se il client non esiste, non fa niente e ritorna 0. Altrimenti ritorna il valore di full_send
int send_to_client(int client_id, const char *message) {
    if(client_id < 0 || client_id >= MAX_CLIENTS) {
        return 0;
    }
    pthread_mutex_lock(&mutex_clients[client_id]);
    if(clients[client_id] == NULL) {
        pthread_mutex_unlock(&mutex_clients[client_id]);
        return 0;
    }
    int client_socket = clients[client_id]->client_socket;
    int ret = full_send(client_socket, message);
    pthread_mutex_unlock(&mutex_clients[client_id]);
    return ret;
}

// funzione che rimuove client dalla lista dei clients, eventualmente anche dalla partita e eventualmente rimuove anche la partita. Se qualche struttura è già vuota, non fa niente
void disconnect_client(int client_id) {
    // prendo le informazioni del client
    pthread_mutex_lock(&mutex_clients_array); // 1 lock per rimozione client
    pthread_mutex_lock(&mutex_games_array); // 2 lock per eventuale eliminazione partita
    pthread_mutex_lock(&mutex_clients[client_id]); // 4 lock per rimozione client specifico
    if(clients[client_id] == NULL) { //. se il client è già stato rimosso, non fa niente
        pthread_mutex_unlock(&mutex_clients[client_id]);
        pthread_mutex_unlock(&mutex_games_array);
        pthread_mutex_unlock(&mutex_clients_array);
        return;
    }
    int client_socket = clients[client_id]->client_socket;
    int game_index = clients[client_id]->game_index;
    int player_index = clients[client_id]->player_index;
    // chiudo la connessione e pulisco il client
    close(client_socket);
    remove_client(client_id);
    console_printf("(W %d) Client disconnesso", client_id);
    pthread_mutex_unlock(&mutex_clients[client_id]); // 4 unlock per rimozione client specifico prima di fare 3 lock per eliminazione player
    // se è connesso a una partita, pulisco il player e eventualmente elimino la partita
    if(game_index != -1) {
        pthread_mutex_lock(&mutex_games[game_index]); // 3 lock per eliminazione player e game
        remove_player(games[game_index], player_index);
        console_printf("(W %d) Player rimosso dalla partita %d", client_id, game_index);
        remove_game_from_list(game_index); // se ci sono altri players non fa niente
        if(games[game_index] == NULL) {
            console_printf("(W %d) Ultimo player della partita: partita %d rimossa", client_id, game_index);
        }
        pthread_mutex_unlock(&mutex_games[game_index]); // 3 unlock per eliminazione player e game
    }
    pthread_mutex_unlock(&mutex_games_array); // 2 unlock per eventuale eliminazione partita
    pthread_mutex_unlock(&mutex_clients_array); // 1 unlock per rimozione client
}

// funzione per controllare che l'input sia name e password
int is_name_password(char *name, char *password) {
    if(name == NULL || password == NULL) {
        return 0;
    }
    if(!str_is_alnum(name) || !str_is_alnum(password)) {
        return 0;
    }
    if(!str_is_len(name, MIN_PLAYER_NAME_LEN, MAX_PLAYER_NAME_LEN) || !str_is_len(password, MIN_PLAYER_PASSWORD_LEN, MAX_PLAYER_PASSWORD_LEN)) {
        return 0;
       }
    return 1;
}



// ############## FUNZIONI PER IL CODICE DEL SERVER ##############
// funzione che inizializza tutte le strutture necessarie al server
void server_init() {
    // inizializzazione della console
    console_init();

    // mutex per gestire le strutture globalmente
    pthread_mutex_init(&mutex_clients_array, NULL); // mutex per la lista dei client (rank 1)
    pthread_mutex_init(&mutex_games_array, NULL); // mutex per la lista dei games (rank 2)

    // mutex per gestire i singoli elementi delle strutture
    for(int i = 0; i < MAX_GAMES; i++) {
        games[i] = NULL;
        pthread_mutex_init(&mutex_games[i], NULL); // mutex per ogni singolo game (rank 3)
    }
    for(int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = NULL;
        pthread_mutex_init(&mutex_clients[i], NULL); // mutex per ogni singolo client (rank 4)
    }
    // mutex per stop_listener_and_timer
    pthread_mutex_init(&mutex_stop_listener_and_timer, NULL); // mutex per stop_listener_and_timer (rank 5)

    // inizializzazione delle variabili globali
    current_games_count = 0;
    next_game_to_create = 0;
    current_clients_count = 0;
    next_client_to_create = 0;

    // strutture dati per gestire il listener
    stop_listener_and_timer = 0;
}

// funzione che distrugge tutte le strutture del server
void server_delete() {
    // distruggere mutex
    pthread_mutex_destroy(&mutex_stop_listener_and_timer);

    for(int i = 0; i < MAX_GAMES; i++) {
        pthread_mutex_destroy(&mutex_games[i]);
    }
    pthread_mutex_destroy(&mutex_games_array);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        pthread_mutex_destroy(&mutex_clients[i]);
    }
    pthread_mutex_destroy(&mutex_clients_array);
    console_restore();
    
}

// funzione che avvia il server. Questa funzione deve creare un thread listener che esegue la funzione listener_thread_func
void start_server(uint16_t port) {
    console_printf("Server in avviamento sulla porta %d", port);
    pthread_mutex_lock(&mutex_stop_listener_and_timer);
    stop_listener_and_timer = 0;
    pthread_mutex_unlock(&mutex_stop_listener_and_timer);

    uint16_t *port_ptr = malloc(sizeof(uint16_t));
    *port_ptr = port;
    pthread_create(&listener_thread, NULL, listener_thread_func, (void *)port_ptr);
    console_printf("Listener thread creato");
    // crea il thread timer
    pthread_create(&timer_thread, NULL, timer_thread_func, NULL);
    console_printf("Timer thread creato");

}

// funzione che ferma il server. Questa funzione deve settare la variabile stop_listener_and_timer a 1 e aspettare che il listener thread termini
void stop_server() {
    console_printf("Terminazione server in corso...");
    pthread_mutex_lock(&mutex_stop_listener_and_timer);
    stop_listener_and_timer = 1;
    pthread_mutex_unlock(&mutex_stop_listener_and_timer);

    pthread_join(listener_thread, NULL);
    console_printf("Listener thread terminato");
    pthread_join(timer_thread, NULL);
    console_printf("Timer thread terminato");
    // terminare tutti i client ed eventualmente le partite
    pthread_mutex_lock(&mutex_clients_array);
    pthread_mutex_lock(&mutex_games_array);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        // manda messaggio di terminazione a tutti i client
        send_to_client(i, "END@Il server è stato terminato");
    }
    pthread_mutex_unlock(&mutex_games_array);
    pthread_mutex_unlock(&mutex_clients_array);

}

// funzione che gestisce il thread listener
void *listener_thread_func(void *port_ptr) {
    uint16_t server_port = *((uint16_t *)port_ptr);
    free(port_ptr);
    // SOCKET
    int server_socket = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0); // crea una socket non bloccante
    // Permette di riutilizzare la porta anche se il server non è stato chiuso correttamente in precedenza
	const int enable = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable));
    if(server_socket < 0) {
        console_printf("(Listener thread) Errore: impossibile creare il socket del server");
        pthread_exit(NULL);
    }
    console_printf("(Listener thread) Creazione server socket");

    // BIND
    struct sockaddr_in server_address; // creo la struttura per l'indirizzo del server
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_port);
    if(bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        console_printf("(Listener thread) Errore: impossibile fare il bind del socket del server");
        pthread_exit(NULL);
    }
    console_printf("(Listener thread) Bind del socket all'indirizzo %s:%d", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));

    // LISTEN
    if(listen(server_socket, LISTENER_MAX_QUEUE) < 0) {
        console_printf("(Listener thread) Errore: impossibile fare il listen del socket del server");
        pthread_exit(NULL);
    }
    console_printf("(Listener thread) Server socket in ascolto");

    // ACCEPT
    while(1) {
        pthread_mutex_lock(&mutex_stop_listener_and_timer);
        if(stop_listener_and_timer) {
            pthread_mutex_unlock(&mutex_stop_listener_and_timer);
            console_printf("(Listener thread) richiesta di terminazione");
            break;
        }
        pthread_mutex_unlock(&mutex_stop_listener_and_timer);
        // controllo se ci sono richieste di connessione usando select, in modo da controllare stop_listener_and_timer ogni tot secondi
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        struct timeval timeout;
        timeout.tv_sec = LISTENER_STOP_CHECK_SECS;
        timeout.tv_usec = LISTENER_STOP_CHECK_USECS;
        int ready = select(server_socket + 1, &readfds, NULL, NULL, &timeout);
        if(ready < 0) {
            console_printf("(Listener thread) Errore: select ha restituito un valore negativo");
            continue;
        } else if(ready == 0) {
            continue;
        } else if(FD_ISSET(server_socket, &readfds)) {
            struct sockaddr_in client_address;
            socklen_t client_address_len = sizeof(client_address);
            int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
            if(client_socket < 0) {
                console_printf("(Listener thread) Errore: impossibile accettare la connessione da %s:%d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                continue;
            }
            console_printf("(Listener thread) Connessione accettata da: %s:%d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
            // creare un nuovo client e un nuovo thread per gestirlo
            pthread_mutex_lock(&mutex_clients_array);
            int client_id = add_client(client_socket);
            int *client_id_ptr = malloc(sizeof(int));
            *client_id_ptr = client_id;
            pthread_create(&clients[client_id]->worker_thread, NULL, worker_thread_func, (void *)client_id_ptr);
            pthread_detach(clients[client_id]->worker_thread); // il thread worker si gestisce da solo
            pthread_mutex_unlock(&mutex_clients_array);
            console_printf("(Listener thread) Creato nuovo client con id %d e worker thread creato con successo", client_id);
        }
    }
    close(server_socket);
    pthread_exit(NULL);
}

// funzione che gestisce il thread worker
void *worker_thread_func(void *client_id_ptr) {
    // prende l'id del client passato come argomento
    int client_id = *((int *)client_id_ptr);
    free(client_id_ptr);
    console_printf("(W %d) Thread creato", client_id);
    // prende il socket del client
    int client_socket = clients[client_id]->client_socket; // no mutex perché il client è appena stato creato
    while(1) {
        // riceve messaggio dal client
        char *client_message = full_recv(client_socket); // no mutex perché il thread worker è l'unico a leggere da questo socket
        if(client_message == NULL) { // se il client ha chiuso la connessione
            console_printf("(W %d) Errore: non si è riuscito a ricevere il messaggio", client_id);
            break;
        }
        console_printf("(W %d) Messaggio ricevuto dal client: %s", client_id, client_message);
        // sposta message in una variabile locale per poter deallocare client_message
        char message[strlen(client_message)+1];
        strcpy(message, client_message);
        free(client_message);

        // se il messaggio è end, termina il thread
        if(strcmp(message, "end") == 0) {
            console_printf("(W %d) Richiesta di terminazione", client_id);
            break;
        }
        console_printf("(W %d) Elaborazione messaggio: %s", client_id, message);
        char *answer = elaborate_message(client_id, message); // elabora il messaggio del client e calcola la risposta
        // invia risposta al client
        if(!send_to_client(client_id, answer)) {
            console_printf("(W %d) Errore: non si è riuscito a inviare la risposta al client", client_id);
            free(answer);
            break;
        }
        console_printf("(W %d) Risposta inviata al client: %s", client_id, answer);
        free(answer);
    }
    // terminazione del thread
    disconnect_client(client_id);
    console_printf("(W %d) Thread exit", client_id);
    pthread_exit(NULL);
}

// funzione che invia un messaggio di informazioni a tutti i giocatori di una partita tranne player_index. Se player_index non è tra 0 e MAX_PLAYERS_PER_GAME-1, invia a tutti i giocatori della partita
void send_info_to_all_players(int game_index, int player_index, const char *message) {
    pthread_mutex_lock(&mutex_games[game_index]);
    for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
        if(i != player_index) {
            send_to_client(games[game_index]->players[i].client_id, message);
        }
    }
    pthread_mutex_unlock(&mutex_games[game_index]);
}

// funzione che costruisce una stringa con tutte le partite disponibili con pattern "room_index room_scenery num_players\n"
char *get_active_games_list() {
    pthread_mutex_lock(&mutex_games_array);
    if(current_games_count == 0) {
        pthread_mutex_unlock(&mutex_games_array);
        return str_malloc("Non ci sono partite attive");
    }
    pthread_mutex_unlock(&mutex_games_array);
    char *game_list = str_malloc("room_index room_scenery num_players");
    for(int i = 0; i < MAX_GAMES; i++) {
        pthread_mutex_lock(&mutex_games[i]);
        if(games[i] != NULL) {
            char *game_info = str_malloc("\n%10d %12d %11d", i, games[i]->scenery, games[i]->players_count);
            game_list = realloc(game_list, strlen(game_list) + strlen(game_info) + 1);
            strcat(game_list, game_info);
            free(game_info);
        }
        pthread_mutex_unlock(&mutex_games[i]);
    }
    return game_list;
}

// funzione che aggiunge un player alla partita games[game_index] e copia i dati del client clients[client_id] nel player
int add_client_to_game(int client_id, int game_index) {
    // prende le info del client
    pthread_mutex_lock(&mutex_games[game_index]);
    pthread_mutex_lock(&mutex_clients[client_id]);
    if(clients[client_id] == NULL || games[game_index] == NULL) { // se il client o la partita non esistono, non fare niente
        pthread_mutex_unlock(&mutex_clients[client_id]);
        pthread_mutex_unlock(&mutex_games[game_index]);
        return -1;
    }
    char name[MAX_PLAYER_NAME_LEN+1];
    strcpy(name, clients[client_id]->name);
    // aggiunge il client alla partita
    int player_index = add_player(games[game_index]);
    if(player_index == -1) { // se la partita è piena non aggiungere il client
        pthread_mutex_unlock(&mutex_clients[client_id]);
        pthread_mutex_unlock(&mutex_games[game_index]);
        return -1;
    }
    // inizializza il player con le info del client
    games[game_index]->players[player_index].client_id = client_id;
    strcpy(games[game_index]->players[player_index].name, name);
    // aggiorna le info del client
    clients[client_id]->game_index = game_index;
    clients[client_id]->player_index = player_index;
    pthread_mutex_unlock(&mutex_clients[client_id]);
    pthread_mutex_unlock(&mutex_games[game_index]);
    return player_index;
}

// funzione che gestisce la risposta del client quando è in stato CLIENT_NOT_LOGGED
char *handle_client_not_logged(int client_id, char *client_message) {
    if(strcmp(client_message, FIRST_CONNECTION_MESSAGE) == 0) {
        return str_malloc("ANSWER@Benvenuto! Digita 'signup' per registrarti o 'login' per accedere");
    } else if(strcmp(client_message, "signup") == 0) {
        change_client_status(client_id, CLIENT_SIGNUP_REQUESTED);
        return str_malloc("ANSWER@Digita il tuo nome e password separati da uno spazio.\n"
        "Sia nome che password devono contenere solo caratteri alfanumerici.\n"
        "Il nome deve essere lungo tra %d e %d caratteri.\n"
        "La password deve essere lunga tra %d e %d caratteri",
        MIN_PLAYER_NAME_LEN, MAX_PLAYER_NAME_LEN, MIN_PLAYER_PASSWORD_LEN, MAX_PLAYER_PASSWORD_LEN);
    } else if(strcmp(client_message, "login") == 0) {
        change_client_status(client_id, CLIENT_LOGIN_REQUESTED);
        return str_malloc("ANSWER@Digita il tuo nome e password separati da uno spazio.");
    }
    return str_malloc("ANSWER@Comando non riconosciuto");
}

// funzione che gestisce la risposta del client quando è in stato CLIENT_SIGNUP_REQUESTED
char *handle_client_signup_requested(int client_id, char *client_message) {
    char *name = my_strtok(client_message, " ");
    char *password = my_strtok(NULL, "");
    if(!is_name_password(name, password)) {
        return str_malloc("ANSWER@Errore: nome utente o password non validi");
    }
    // Apri il file degli utenti
    FILE *users_file = fopen("users.txt", "r");
    if(users_file == NULL) {
        return str_malloc("ANSWER@Errore: impossibile aprire il file degli utenti");
    }
    // Controlla se il nome del client esiste già nel file users.txt
    char line[MAX_MSG_LEN];
    int already_exists = 0;
    while(fgets(line, MAX_MSG_LEN, users_file) != NULL) {
        char *file_name = my_strtok(line, " ");
        if(strcmp(name, file_name) == 0) {
            already_exists = 1;
            break;
        }
    }
    fclose(users_file);
    // Se esiste già, invia messaggio di errore
    if(already_exists) {
        return str_malloc("ANSWER@Errore: nome utente già esistente");
    }
    // Se non esiste già, salva il nome e la password del client nel file users.txt
    users_file = fopen("users.txt", "a");
    if(users_file == NULL) {
        return str_malloc("ANSWER@Errore: impossibile aprire il file degli utenti");
    }
    fprintf(users_file, "%s %s\n", name, password);
    fclose(users_file);
    // salvare il nome del client
    strcpy(clients[client_id]->name, name);
    // cambia stato
    change_client_status(client_id, CLIENT_LOGGED);
    // prepara la risposta
    char *game_list = get_active_games_list();
    char *answer = str_malloc("ANSWER@Registrazione effettuata con successo.\n"
    "Iniziare una partita usando il comando start oppure start [room_index]\n"
    "per aggiungerti a una già esistente.\n"
    "LISTA DELLE PARTITE ESISTENTI:\n%s", game_list);
    free(game_list);
    return answer;
}

// funzione che gestisce la risposta del client quando è in stato CLIENT_LOGIN_REQUESTED
char *handle_client_login_requested(int client_id, char *client_message) {
    char *name = my_strtok(client_message, " ");
    char *password = my_strtok(NULL, "");
    if(!is_name_password(name, password)) {
        return str_malloc("ANSWER@Errore: nome utente o password non validi");
    }
    // Apri il file degli utenti
    FILE *users_file = fopen("users.txt", "r");
    if(users_file == NULL) {
        return str_malloc("ANSWER@Errore: impossibile aprire il file degli utenti");
    }
    // Controlla se nome e password sono presenti in users.txt
    char line[MAX_MSG_LEN];
    int found = 0;
    while(fgets(line, MAX_MSG_LEN, users_file) != NULL) {
        char *file_name = my_strtok(line, " ");
        char *file_password = my_strtok(NULL, "\n");
        if(strcmp(name, file_name) == 0 && strcmp(password, file_password) == 0) {
            found = 1;
            break;
        }
    }
    fclose(users_file);
    // Se non trovato, invia messaggio di errore
    if(!found) {
        return str_malloc("ANSWER@Errore: nome utente o password non validi");
    }
    // Se già loggato, invia messaggio di errore
    pthread_mutex_lock(&mutex_clients_array);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i] != NULL && strcmp(clients[i]->name, name) == 0) {
            pthread_mutex_unlock(&mutex_clients_array);
            return str_malloc("ANSWER@Errore: utente già loggato");
        }
    }
    // Se trovato, salva il nome del client
    strcpy(clients[client_id]->name, name);
    pthread_mutex_unlock(&mutex_clients_array);
    // cambia stato
    change_client_status(client_id, CLIENT_LOGGED);
    // prepara la risposta
    char *game_list = get_active_games_list();
    char *answer = str_malloc("ANSWER@Accesso effettuato con successo.\n"
    "Iniziare una partita usando il comando start oppure start [room_index] per aggiungerti a una già esistente.\n"
    "LISTA DELLE PARTITE ESISTENTI:\n%s", game_list);
    free(game_list);
    return answer;
}

// funzione che gestisce la risposta del client quando è in stato CLIENT_LOGGED
char *handle_client_logged(int client_id, char *client_message) {
    char *command = my_strtok(client_message, " ");
    char *game_index_str = my_strtok(NULL, "");
    if(strcmp(command, "start") != 0) {
        char *game_list = get_active_games_list();
        char *answer = str_malloc("ANSWER@Errore: Digita start oppure start"
        "\nseguito da un room_index della seguente tabella\n"
        "LISTA DELLE PARTITE ESISTENTI:\n%s", game_list);
        free(game_list);
        return answer;
    }
    // se non è specificato l'indice della partita, scegliere un nuovo scenario
    if(game_index_str == NULL) {
        // scegliere un nuovo scenario
        change_client_status(client_id, CLIENT_CHOOSE_SCENERY);
        return str_malloc("ANSWER@Scegli uno scenario tra quelli disponibili"
        "\n\n********************** SCENARI DISPONIBILI **********************"
        "\n0 - "SCENERY_0_NAME);
    }
    // se l'indice della partita non è un numero, errore
    if(!str_is_num(game_index_str)) {
        char *game_list = get_active_games_list();
        char *answer = str_malloc("ANSWER@Errore: l'indice della partita deve essere un numero\n"
        "LISTA DELLE PARTITE ESISTENTI:\n%s", game_list);
        free(game_list);
        return answer;
    }
    int game_index = atoi(game_index_str);
    // se l'indice della partita non è nell'intervallo, errore
    if(game_index < 0 || game_index >= MAX_GAMES) {
        char *game_list = get_active_games_list();
        char *answer = str_malloc("ANSWER@Errore: indice della partita non valido\n"
        "LISTA DELLE PARTITE ESISTENTI:\n%s", game_list);
        free(game_list);
        return answer;
    }
    pthread_mutex_lock(&mutex_games[game_index]);
    // se la partita non esiste, errore
    if(games[game_index] == NULL) {
        pthread_mutex_unlock(&mutex_games[game_index]);
        char *game_list = get_active_games_list();
        char *answer = str_malloc("ANSWER@Errore: la partita non esiste\n"
        "LISTA DELLE PARTITE ESISTENTI:\n%s", game_list);
        free(game_list);
        return answer;
    }
    pthread_mutex_unlock(&mutex_games[game_index]);
    // aggiungere il client alla partita. Se la partita è piena, torna -1
    int player_index = add_client_to_game(client_id, game_index);
    // se la partita è piena, errore
    if(player_index == -1) {
        char *game_list = get_active_games_list();
        char *answer = str_malloc("ANSWER@Errore: la partita è piena\n"
        "LISTA DELLE PARTITE ESISTENTI:\n%s", game_list);
        free(game_list);
        return answer;
    }
    // cambiare lo stato del client
    change_client_status(client_id, CLIENT_PLAYING);
    //mandare a tutti i giocatori della partita il messaggio che il nuovo giocatore è entrato
    char *info = str_malloc("INFO@Il giocatore %s è entrato nella partita", clients[client_id]->name);
    console_printf("(W %d) %s", client_id, info);
    // preparare la risposta
    char *answer = str_malloc("ANSWER@Sei entrato nella partita %d"
    "\n\n********************** REGOLE DEL GIOCO **********************"
    "\nL'obiettivo del gioco è scappare dalla stanza raccogliendo tutti"
    "\n i token prima che il tempo scada"
    "\n\n************************** COMANDI ***************************"
    "\n- look [location | object] - se non c'è nessun argomento, descrivi la location"
    "\nattuale. Se l'argomento è una location in cui puoi andare, ti sposti"
    "\nin quella location e la descrivi. Se l'argomento è un oggetto nello zaino"
    "\no nella location attuale, lo esamini"
    "\n\n- take object - raccogli un oggetto dalla location attuale. Se un oggetto"
    "\nè bloccato, potresti comunque raccoglierlo, ma dovrai risolvere un enigma"
    "\n\n- drop object - lascia un oggetto nella location attuale"
    "\n\n- use object1 [object2] - usa un oggetto (dipende dalla location) o il"
    "\nprimo oggetto sul secondo. Il primo oggetto deve"
    "\nessere posseduto, il secondo deve essere o posseduto o nella location attuale"
    "\n\n- objs - mostra gli oggetti posseduti"
    "\n\n- chat message - invia un messaggio a tutti i giocatori"
    "\n\n- end - termina la partita e la connessione", game_index);
    // controlla eventuali azioni da fare appena il player entra nella partita
    Game *game = games[game_index];
    int scenery = game->scenery;
    switch(scenery) {
        case 0: {
            // se nel corridoio ci sono 2 giocatori, avvia il timer del corridoio
            int players_in_corridoio = 0;
            for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
                if(game->players[i].current_location == 0) {
                    players_in_corridoio++;
                }
            }
            if(players_in_corridoio == 2 && get_timer_id(game, "corridoio") == -1) {
                add_timer_to_game(game, 10, "END@Il tempo è scaduto, il pavimento del corridoio è crollato. Avete perso", "corridoio");
                // aggiungi info a tutti i giocatori della partita che il timer è stato avviato
                info = realloc(info, strlen(info) + strlen("\nAdesso siete troppi nel corridoio, se non vi muovete entro 10 secondi il pavimento crollerà") + 1);
                strcat(info, "\nAdesso siete troppi nel corridoio, se non vi muovete entro 10 secondi il pavimento crollerà");
                // aggiungi info al player che il timer è stato avviato
                answer = realloc(answer, strlen(answer) + strlen("\nSei in un corridoio con altri giocatori."
                "\nIl corridoio è fragile e se rimarrete qui per più di 10 secondi il pavimento del corridoio crollerà") + 1);
                strcat(answer, "\nSei in un corridoio con altri giocatori."
                "\nIl corridoio è fragile e se rimarrete qui per più di 10 secondi il pavimento del corridoio crollerà");
            }
            break;
        }
    }
    for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
        if(game->players[i].name[0] != '\0') {
            send_to_client(game->players[i].client_id, info);
        }
    }
    free(info);
    return answer;
}

// funzione che gestisce la risposta del client quando è in stato CLIENT_CHOOSE_SCENERY
char *handle_client_choose_scenery(int client_id, char *client_message) {
    // se il client non ha inserito un numero, errore
    if(!str_is_num(client_message)) {
        return str_malloc("ANSWER@Errore: lo scenario deve essere un numero");
    }
    int scenery = atoi(client_message);
    // se lo scenario non è valido, errore
    if(scenery < 0 || scenery >= SCENERY_COUNT) {
        return str_malloc("ANSWER@Errore: scenario non valido");
    }
    // creare una nuova partita con lo scenario scelto
    int game_index = add_game_to_list(scenery);
    // se non si riesce a creare la partita, errore
    if(game_index < 0) {
        return str_malloc("ANSWER@Errore: numero massimo di partite raggiunto");
    }
    // aggiungere il client alla partita. Non torna -1 perché la partita è appena stata creata
    add_client_to_game(client_id, game_index);
    // cambiare lo stato del client
    change_client_status(client_id, CLIENT_PLAYING);
    // manda a tutti i giocatori della partita il messaggio che il nuovo giocatore è entrato
    // preparare la risposta con le regole del gioco
    return str_malloc("ANSWER@Sei entrato nella partita %d"
    "\n\n********************** REGOLE DEL GIOCO **********************"
    "\nL'obiettivo del gioco è scappare dalla stanza raccogliendo tutti"
    "\n i token prima che il tempo scada"
    "\n\n************************** COMANDI ***************************"
    "\n- look [location | object] - se non c'è nessun argomento, descrivi la location"
    "\nattuale. Se l'argomento è una location in cui puoi andare, ti sposti"
    "\nin quella location e la descrivi. Se l'argomento è un oggetto nello zaino"
    "\no nella location attuale, lo esamini"
    "\n\n- take object - raccogli un oggetto dalla location attuale. Se un oggetto"
    "\nè bloccato, potresti comunque raccoglierlo, ma dovrai risolvere un enigma"
    "\n\n- drop object - lascia un oggetto nella location attuale"
    "\n\n- use object1 [object2] - usa un oggetto (dipende dalla location) o il"
    "\nprimo oggetto sul secondo. Il primo oggetto deve"
    "\nessere posseduto, il secondo deve essere o posseduto o nella location attuale"
    "\n\n- objs - mostra gli oggetti posseduti"
    "\n\n- chat message - invia un messaggio a tutti i giocatori"
    "\n\n- end - termina la partita e la connessione", game_index);
}

// funzione che gestisce la risposta del client quando è in stato CLIENT_PLAYING
char *handle_client_playing(int client_id, char *client_message) {
    char *command = my_strtok(client_message, " ");
    char *args = my_strtok(NULL, "");
    char *answer = NULL;
    char *info = NULL;
    pthread_mutex_lock(&mutex_clients[client_id]);
    Game *game = games[clients[client_id]->game_index];
    int player_index = clients[client_id]->player_index;
    int game_index = clients[client_id]->game_index;
    pthread_mutex_unlock(&mutex_clients[client_id]);
    pthread_mutex_lock(&mutex_games[game_index]);
    game_action(game, player_index, command, args, &answer, &info);
    pthread_mutex_unlock(&mutex_games[game_index]);
    if(answer == NULL) {
        answer = str_malloc("ANSWER@Errore: impossibile elaborare il comando");
    }
    if(info != NULL) {
        // invia info a tutti i giocatori tranne il mittente
        send_info_to_all_players(game_index, player_index, info);
        free(info);
    }
    // aggiungere i token rimasti e il tempo rimanente e il nome dei giocatori alla risposta
    char *tokens_str = str_malloc("\nToken presi: %d/%d    ", game->obtained_tokens, game->token_count);
    char *time_str = str_malloc("Tempo rimanente: %d secondi", timer_remaining(get_timer_id(game, "main")));
    char *players_str = str_malloc("    Giocatori: ");
    for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
        if(game->players[i].client_id != -1) {
            players_str = realloc(players_str, strlen(players_str) + strlen(game->players[i].name) + 1);
            strcat(players_str, game->players[i].name);
            strcat(players_str, " ");
        }
    }
    answer = realloc(answer, strlen(answer) + strlen(tokens_str) + strlen(time_str) + strlen(players_str) + 1);
    strcat(answer, tokens_str);
    strcat(answer, time_str);
    strcat(answer, players_str);
    free(tokens_str);
    free(time_str);
    free(players_str);
    return answer;
        
}
    
// funzione che elabora il messaggio del client e calcola la risposta
char *elaborate_message(int client_id, char *client_message) {
    char *answer = NULL;
    pthread_mutex_lock(&mutex_clients[client_id]);
    int client_status = clients[client_id]->status;
    pthread_mutex_unlock(&mutex_clients[client_id]);
    switch(client_status) {
        case CLIENT_NOT_LOGGED: {
            return handle_client_not_logged(client_id, client_message);
        }
        case CLIENT_SIGNUP_REQUESTED: {
            return handle_client_signup_requested(client_id, client_message);
        }
        case CLIENT_LOGIN_REQUESTED: {
            return handle_client_login_requested(client_id, client_message);
        }
        case CLIENT_LOGGED: {
            return handle_client_logged(client_id, client_message);
        }
        case CLIENT_CHOOSE_SCENERY: {
            return handle_client_choose_scenery(client_id, client_message);
        }
        case CLIENT_PLAYING: {
            return handle_client_playing(client_id, client_message);
        }
    }
    if(answer == NULL) {
        answer = str_malloc("ANSWER@Errore: impossibile elaborare il messaggio");
    }
    return answer;
}

// funzione che stampa tutte le info di un client a una certa tabulazione
void print_client_info(int client_id, int n_spaces) {
    pthread_mutex_lock(&mutex_clients[client_id]);
    if(clients[client_id] == NULL) {
        console_printf("%*sClient %d: NULL", n_spaces, "", client_id);
    } else {
        console_printf("%*sClient %d:", n_spaces, "", client_id);
        console_printf("%*sWorker thread: %lu", n_spaces + 2, "", clients[client_id]->worker_thread);
        console_printf("%*sClient socket: %d", n_spaces + 2, "", clients[client_id]->client_socket);
        console_printf("%*sName: %s", n_spaces + 2, "", clients[client_id]->name);
        console_printf("%*sStatus: %d", n_spaces + 2, "", clients[client_id]->status);
        console_printf("%*sGame index: %d", n_spaces + 2, "", clients[client_id]->game_index);
        console_printf("%*sPlayer index: %d", n_spaces + 2, "", clients[client_id]->player_index);
    }
    pthread_mutex_unlock(&mutex_clients[client_id]);
}

// funzione che stampa tutte le info di un game a una certa tabulazione
void print_game_info(int game_index, int n_spaces) {
    pthread_mutex_lock(&mutex_games[game_index]);
    if(games[game_index] == NULL) {
        console_printf("%*sGame %d: NULL", n_spaces, "", game_index);
    } else {
        console_printf("%*sGame %d:", n_spaces, "", game_index);

        console_printf("%*sScenery: %d", n_spaces + 2, "", games[game_index]->scenery);
        console_printf("%*sLocations count: %d", n_spaces + 2, "", games[game_index]->locations_count);
        console_printf("%*sStart location: %d", n_spaces + 2, "", games[game_index]->start_location);
        console_printf("%*sObjects count: %d", n_spaces + 2, "", games[game_index]->objects_count);
        console_printf("%*sPlayers count: %d", n_spaces + 2, "", games[game_index]->players_count);
        console_printf("%*sNext player to create: %d", n_spaces + 2, "", games[game_index]->next_player_to_create);
        console_printf("%*sToken count: %d", n_spaces + 2, "", games[game_index]->token_count);
        console_printf("%*sObtained tokens: %d", n_spaces + 2, "", games[game_index]->obtained_tokens);

        // players
        for(int i = 0; i < MAX_PLAYERS_PER_GAME; i++) {
            console_printf("%*sPlayer %d:", n_spaces + 2, "", i);
            console_printf("%*sName: %s", n_spaces + 4, "", games[game_index]->players[i].name);
            console_printf("%*sClient id: %d", n_spaces + 4, "", games[game_index]->players[i].client_id);
            console_printf("%*sCurrent location: %d", n_spaces + 4, "", games[game_index]->players[i].current_location);
            console_printf("%*sCurrent Enigma: %d", n_spaces + 4, "", games[game_index]->players[i].current_enigma);
        }
        // locations
        for(int i = 0; i < games[game_index]->locations_count; i++) {
            console_printf("%*sLocation %d:", n_spaces + 2, "", i);
            console_printf("%*sName: %s", n_spaces + 4, "", games[game_index]->locations[i].name);
            console_printf("%*sDescription: %s", n_spaces + 4, "", games[game_index]->locations[i].description);
            // adjacent locations
            char *adj_loc_str = str_malloc("Adjacent locations: ");
            for(int j = 0; j < games[game_index]->locations[i].adjacent_locations_count; j++) {
                char *loc_str = str_malloc("%d ", games[game_index]->locations[i].adjacent_locations[j]);
                adj_loc_str = realloc(adj_loc_str, strlen(adj_loc_str) + strlen(loc_str) + 1);
                strcat(adj_loc_str, loc_str);
                free(loc_str);
            }
            console_printf("%*s%s", n_spaces + 4, "", adj_loc_str);
                
        }
        // objects
        for(int i = 0; i < games[game_index]->objects_count; i++) {
            console_printf("%*sObject %d:", n_spaces + 2, "", i);
            console_printf("%*sName: %s", n_spaces + 4, "", games[game_index]->objects[i].name);
            console_printf("%*sDescription: %s", n_spaces + 4, "", games[game_index]->objects[i].description);
            console_printf("%*sStatus: %d", n_spaces + 4, "", games[game_index]->objects[i].status);
            console_printf("%*sPosition index: %d", n_spaces + 4, "", games[game_index]->objects[i].position_index);
        }
        // timers è una lista di Timer
        Timer *q = games[game_index]->timers_list;
        int counter = 0;
        while(q != NULL) {
            console_printf("%*sTimer: %d", n_spaces + 2, "", counter);
            console_printf("%*sTimer name: %s", n_spaces + 4, "", q->timer_name);
            console_printf("%*sTimer id: %d", n_spaces + 4, "", q->timer_id);
            console_printf("%*sMessage: %s", n_spaces + 4, "", q->args->message);
            q = q->next;
            counter++;
        }

    }
    pthread_mutex_unlock(&mutex_games[game_index]);
}



// ############## MAIN ##############
int main(int argc, char *argv[]) {
    // Controllo input argomenti
    uint16_t port = 0; // porta del server
    int server_status = SERVER_STOPPED;
    if(argc > 2) { // controllo se ci sono troppi argomenti
        fprintf(stderr, "Errore: troppi argomenti\n Uso: %s [port]\n", argv[0]);
        exit(1);
    } else if(argc == 2) { // controllo se c'è un argomento, ovvero la port
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

    server_init();

    if(argc == 2) {
        server_status = SERVER_STARTED;
        start_server(port);
    } else {
        console_printf("\n********************** SERVER INIZIATO **********************\n"
        "Comandi disponibili:\n"
        "start [port] - avvia il server sulla porta [port] (default %d)\n"
        "stop - ferma il server\n"
        "exit - esce dal programma\n"
        "*************************************************************", DEFAULT_PORT);
        
    }

    while(1) {
        char command_buffer[MAX_MSG_LEN];
        console_in(command_buffer);
        // separo il comando dall'argomento
        char *command = my_strtok(command_buffer, " ");
        char *arg = my_strtok(NULL, " ");

        if(strcmp(command, "start") == 0) {
            if(server_status == SERVER_STARTED) {
                console_printf("Errore: il server è già partito");
            } else {
                // controllo se argomento è una port
                if(arg == NULL) {
                    port = DEFAULT_PORT;
                    start_server(DEFAULT_PORT);
                    server_status = SERVER_STARTED;
                } else if(!str_is_num(arg)) {
                    console_printf("Errore: la porta deve essere un numero");
                } else {
                    port = atoi(arg);
                    if(port < MIN_PORT || port > MAX_PORT) {
                        console_printf("Errore: la porta deve essere un numero tra 1 e 65535");
                    } else {
                        start_server(port);
                        server_status = SERVER_STARTED;
                    }
                }
            }
        } else if(strcmp(command_buffer, "stop") == 0) {
            if(server_status == SERVER_STOPPED) {
                console_printf("Errore: il server è già fermo");
            } else if(current_games_count > 0) {
                console_printf("Errore: ci sono partite in corso");
            } else {
                stop_server();
                server_status = SERVER_STOPPED;
            }
        } else if(strcmp(command_buffer, "info") == 0) {
            for(int i = 0; i < MAX_CLIENTS; i++) {
                print_client_info(i, 0);
            }
            for(int i = 0; i < MAX_GAMES; i++) {
                print_game_info(i, 0);
            }
        } else if(strcmp(command_buffer, "exit") == 0) {
            if(server_status == SERVER_STARTED) {
                console_printf("Errore: fermare il server prima di uscire");
            } else {
                server_delete();
                exit(0);
            }
        } else {
            console_printf("Comando non riconosciuto");
        }
    }
}

