/* Modificaciones realizadas sobre codigo provisto por la cátedra */

#ifndef LOGGER_H
#define LOGGER_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Macros y funciones simples para log de errores.
 * EL log se hace en forma simple
 * Alternativa: usar syslog para un log mas completo.
 * Ver sección 13.4 del libro de  Stevens */

typedef enum {DEBUG=0, INFO, LOG_ERROR, FATAL} LOG_LEVEL;

extern LOG_LEVEL current_level;
extern bool error_flag;

/* Minimo nivel de log a registrar.
 * Cualquier llamada a log con un nivel mayor a newLevel sera ignorada */
void setLogLevel(LOG_LEVEL newLevel);

char * levelDescription(LOG_LEVEL level);

void log_print(LOG_LEVEL level, const char *fmt, ...);

#endif