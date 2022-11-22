#include <stdlib.h>
#include "../include/new_parser.h"

void parser_init(struct parser *p)
{
    p->current = p->states[0]->state;
    p->index = 0;
    p->error = 0;
}

enum parser_state parser_feed(struct parser *p, uint8_t b) {
    parser_substate * substate = (p->states[p->index]);

    switch (p->current){
        case single_read:
            *(substate->result) = b;
            if(substate->check_function == NULL){
                p->index = p->index + 1;
                substate = (p->states[p->index]);
                if(p->index >= p->size)
                    p->current = done_read;
                else
                    p->current = substate->state;
                break;
            }


            if (substate->check_function(substate->result, 1, &p->error))   // Check result
            {
                p->index = p->index + 1;
                substate = (p->states[p->index]);
                if(p->index >= p->size)
                    p->current = done_read;
                else
                    p->current = substate->state;
            }
            else
            {
                substate->state = error_read;
            }
            break;

        case read_N:


            p->index = p->index + 1;
            if(p->index >= p->size) {
                p->current = error_read;
                break;
            }

            substate = (p->states[p->index]);
            substate->remaining = b;
            substate->size = b;
            p->current = substate->state;

            if (substate->remaining <= 0)      // Si es cero, pongo un string vacio y salteo la fase de long_read
            {
                substate->result = malloc(1);
                substate->result[0] = 0;
                p->index = p->index + 1;
                if(p->index >= p->size) {
                    p->current = done_read;
                    break;
                } 
                else {
                    substate = (p->states[p->index]);
                    p->current = substate->state;
                }
            } 
            else{
                substate->result = malloc((sizeof(uint8_t) * substate->size)+1);
            }
            break;

        case long_read:

            (substate->result)[(substate->size - substate->remaining)] = b;

            (substate->remaining)--;

            if (substate->remaining <= 0)
            {
                (substate->result)[(substate->size)] = 0;
                if (substate->check_function != NULL)
                {
                    if(!(substate->check_function( substate->result, substate->size, &p->error))) {
                        p->current = error_read;
                        break;
                    }
                }
                p->index = p->index + 1;
                if(p->index >= p->size) {
                    p->current = done_read;
                    break;
                } else {
                    substate = (p->states[p->index]);
                    p->current = substate->state;
                }

            }
            break;
        case done_read:
                /* DONE */
            break;
        case error_read:
                /* ERROR */
            break;
        default:
            abort();
            break;
    }
    return p->current;
}

enum parser_state consume(buffer *b, struct parser *p, bool *error)
{
    enum parser_state st = p->states[0]->state;
    bool finished = false;
    while (buffer_can_read(b) && !finished)
    {
        uint8_t byte = buffer_read(b);
        st = parser_feed(p, byte);
        if (is_done(st, error))
        {
            finished = true;
        }
    }
    return st;
}

uint8_t is_done(const enum parser_state state, bool *error)
{
    bool ret = false;
    switch (state)
    {
        case error_read:
            if (error != 0)
            {
                *error = true;
            }
            ret = true;
            break;
        case done_read:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
    return ret;
}