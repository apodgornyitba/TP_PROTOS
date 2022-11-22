#ifndef METRICS_H
#define METRICS_H

#include <stdio.h>
#include <stdlib.h>
#include <../include/logger.h>

// Structure that stores metrics 
typedef struct metrics_t {
    // Connections
    size_t historic_connections;
    size_t concurrent_connections;
    size_t max_concurrent_connections;
    size_t historic_connections_attempts;
    size_t connecting_clients;

    // Historic
    size_t historic_byte_transfer;
    size_t historic_auth_attempts;

    // Read
    size_t total_reads;
    size_t metrics_average_bytes_per_read;

    // Write
    size_t total_writes;
    size_t metrics_average_bytes_per_write;
} metrics_t;


// Initialize metrics
void init_metrics(metrics_t *m);

// Update metrics
void add_connecting_clients(metrics_t *m);
void end_connecting_clients(metrics_t *m);
void add_connection(metrics_t *m);
void end_connection(metrics_t *m);
void add_bytes(metrics_t *m, int n);
void add_read(metrics_t *m);
void add_write(metrics_t *m);

// Get metrics
int get_current_connections(metrics_t *m);
int get_historic_connections(metrics_t *m);
int get_bytes(metrics_t *m);

#endif