#include "console.h"
#include "constants.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>




// Inizializza la console
void console_init() {
    struct termios t;
    
    // Copio i vecchi valori di configurazione del terminale
	tcgetattr(STDIN_FILENO, &t);

    // Li salvo nella apposita variabile
    console_original_config = t;

    // Cambio le flag:
	t.c_lflag &= ~(ICANON); // Disabilito l'Input Buffer
	t.c_lflag &= ~(ECHO); // Disabilito l'Input Echo

    // Carico i nuovi valori di configurazione
	tcsetattr(STDIN_FILENO, TCSANOW, &t);
    

    if(pthread_mutex_init(&mutex_console, NULL)){
        tcsetattr(STDIN_FILENO, TCSANOW, &console_original_config);
        printf("Errore nell'inizializzazione della console");
        exit(0);
    }

    // Inizializzo il buffer di input
    input_len = 0;
    memset(input_buffer, '\0', MAX_MSG_LEN);
    printf("> ");
    fflush(stdout);
}

// Mette il pthread in sleep fino a quando non viene premuto un tasto
int keyboard_hit(long secs, long microsecs) {
    struct timeval tv = { secs, microsecs };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

// Nasconde il comando che sta venendo scritto
void console_hide_input() {
    int i;

    // Porto il cursore all'inizio della riga
	printf("\r");

    // Cancello tutti i caratteri ("> " + l'input)
	for (i = 0; i < input_len + 2; i++)
		printf(" ");

    // Riporto il cursore all'inizio della riga
	printf("\r");
}

// Mostra il comando che sta venendo scritto
void console_show_input() {
	printf("> %s", input_buffer);
    fflush(stdout);
}

// Prende una stringa in input da stdin di massimo max_len caratteri
void console_in(char *buf) {
    while (1){
        // Prende un carattere da stdin
        char c;
		if(!keyboard_hit(1L, 0L)) {
			continue;
		}
        pthread_mutex_lock(&mutex_console);
        read(STDIN_FILENO, &c, 1);
		char cstr[2];
		cstr[0] = c;
		cstr[1] = '\0';

        if (c == 127) { // Backspace => cancella un carattere
            if (input_len > 0) {
                input_len--;
                input_buffer[input_len] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == '\n') { // Enter => fine dell'input
            if (input_len){
                pthread_mutex_unlock(&mutex_console);
                break;
            }
        } else if (c >= 32 && c <= 126) { // Qualsiasi carattere stampabile => inserisco nell'input buffer
            if (input_len < MAX_MSG_LEN) {
                input_buffer[input_len] = c;
                input_len++;
                input_buffer[input_len] = '\0';
                printf(cstr);
                fflush(stdout);
            }
        }
        pthread_mutex_unlock(&mutex_console);
    }
    strcpy(buf, input_buffer);
    pthread_mutex_lock(&mutex_console);
    printf("\n> ");
    input_len = 0;
    input_buffer[input_len] = '\0';
    pthread_mutex_unlock(&mutex_console);
}

// Stampa una stringa formattata nella console
void console_printf(const char* format, ...) {
    pthread_mutex_lock(&mutex_console);
    console_hide_input();
    // usa la libreria varargs per fare forwarding di printf
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    // Va sempre a capo
    printf("\n");
    console_show_input();
    pthread_mutex_unlock(&mutex_console);
}

// Riporta la console alla configurazione di base
void console_restore() {
    // Rimette i valori di configurazione del terminale di default
    tcsetattr(STDIN_FILENO, TCSANOW, &console_original_config);

    // Distrugge il mutex
    pthread_mutex_destroy(&mutex_console);
}