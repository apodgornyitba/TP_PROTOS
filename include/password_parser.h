    #ifndef PASSWORD_PARSER_H
    #define PASSWORD_PARSER_H

    #include <stdint.h>
    #include <stdbool.h>
    #include <stddef.h>

    #define USER_L 5
    #define PASS_L 10

    enum password_parser_state {
        user_word_search,
        user_read,
        pass_word_search,
        pass_read
    };

    typedef struct password_parser {

        enum password_parser_state current;

        size_t current_index;

        bool last;
        uint8_t *username;
        uint8_t *password;

        struct sockaddr_storage *client;
        struct sockaddr_storage *origin;
        int *userIndex;

    } password_parser;

    /* Initializes password dissectors parser */
    void password_parser_init(struct password_parser *p);

    /* Feed parser. Used in dissec_consume. */
    enum password_parser_state password_parser_feed(struct password_parser *p, uint8_t b);

    /* Feeds the buffer to the parser. Buffer is not */
    void dissec_consume(uint8_t *buffer, size_t size, struct password_parser *parser);

    #endif