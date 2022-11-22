#include "../include/metrics.h"

// Initialize metrics
void init_metrics(metrics_t *m){
    m->historic_connections = 0;
    m->concurrent_connections = 0;
    m->max_concurrent_connections = 0;
    m->historic_connections_attempts = 0;
    m->connecting_clients=0;
    m->historic_byte_transfer = 0;
    m->historic_auth_attempts = 0;
    m->total_reads = 0;
    m->metrics_average_bytes_per_read = 0;
    m->total_writes = 0;
    m->metrics_average_bytes_per_write = 0;
}

// Update metrics
void add_connecting_clients(metrics_t *m){
    (m->connecting_clients)++;
}

void end_connecting_clients(metrics_t *m){
    (m->connecting_clients)--;
}

void add_connection(metrics_t *m){
    (m->historic_connections)++;
    (m->concurrent_connections)++;
    if(m->concurrent_connections >= m->max_concurrent_connections) {
        fprintf(stderr, "No more connections can be added");
        // log_print(LOG_ERROR, "No more connections can be added");
    }
}

void end_connection(metrics_t *m){
    (m->concurrent_connections)--;
    // if(m->concurrent_connections < 0) {
    //     log_error(LOG_ERROR, "No more connections can be removed.");
    // }
}

void add_bytes(metrics_t *m, int n){  
    m->historic_byte_transfer += n;
}

void add_read(metrics_t *m){  
    (m->total_reads)++;
}

void add_write(metrics_t *m){  
    (m->total_writes)++;
}

// Get metrics
int get_current_connections(metrics_t *m){
    return m->concurrent_connections;
}

int get_historic_connections(metrics_t *m){
    return m->historic_connections;
}

int get_bytes(metrics_t *m){
    return m->historic_byte_transfer;
}

int get_connecting_clients(metrics_t *m){
    return m->connecting_clients;
}
