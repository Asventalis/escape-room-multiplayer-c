#ifndef CONSTANTS_H
#define CONSTANTS_H



// ############## COSTANTI SERVER ##############
#define DEFAULT_PORT 8080
#define MAX_GAMES 10
#define MAX_PLAYERS_PER_GAME 10
#define LISTENER_MAX_QUEUE 10
#define MIN_PLAYER_NAME_LEN 3
#define MAX_PLAYER_NAME_LEN 10
#define MIN_PLAYER_PASSWORD_LEN 8
#define MAX_PLAYER_PASSWORD_LEN 30
#define LISTENER_STOP_CHECK_SECS 1L
#define LISTENER_STOP_CHECK_USECS 0L



// ############## COSTANTI CLIENT ##############
#define SERVER_IP "127.0.0.1"



// ############## COSTANTI PROTOCOLLO ##############
#define HEADER_ANSWER "ANSWER" // quando il messaggio del server inizia con "ANSWER" è una risposta ad un comando del client
#define HEADER_INFO "INFO" // quando il messaggio del server inizia con "INFO" è un messaggio con informazioni riguardo il game
#define HEADER_END "END" // quando il messaggio del server inizia con "END" è un messaggio di fine partita
#define HEADER_DELIMITER "@" // delimitatore tra header e messaggio
#define FIRST_CONNECTION_MESSAGE "FIRST CONNECTION" // messaggio inviato dal client al server per la prima connessione


// ############## COSTANTI COMUNI ##############
#define MIN_PORT 1
#define MAX_PORT 65535

// ############## COSTANTI CONSOLE ##############
#define MAX_MSG_LEN 80


// ############## COSTANTI GAME ##############
#define SCENERY_COUNT 1
#define SCENERY_0_NAME "Appartamento con pavimento che crolla"


// ################# DERIVATI ##################
#define MAX_CLIENTS (MAX_GAMES*MAX_PLAYERS_PER_GAME)



#endif // CONSTANTS_H