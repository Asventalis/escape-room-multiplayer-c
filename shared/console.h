#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdio.h>
#include <pthread.h>
#include <termios.h>
#include <stdarg.h>
#include "constants.h"



// ############## VARIABILI GLOBALI ##############

pthread_mutex_t mutex_console;  // Mutex per garantire che non si sovrappongano più scritte
struct termios console_original_config; // Vecchi valori di configurazione del terminale
char input_buffer[MAX_MSG_LEN]; // Buffer di input (quello di sistema su stdin viene disabilitato)
int input_len; // Numero di caratteri nel buffer di input



// ############## FUNZIONI ##############
// Inizializza la console
void console_init();

// Mette il pthread in sleep fino a quando non viene premuto un tasto
int keyboard_hit(long secs, long microsecs);

// Nasconde il comando che sta venendo scritto
void console_hide_input();

// Mostra il comando che sta venendo scritto
void console_show_input();

// Prende una stringa in input da stdin di massimo MAX_MSG_LEN caratteri
void console_in(char* buf);

// Mostra in maniera sicura una stringa
void console_printf(const char* format, ...);

// Riporta la console alla configurazione di base
void console_restore();



#endif // CONSOLE_H