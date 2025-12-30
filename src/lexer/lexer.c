#include "lexer.h"
#include "../utils/utils.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int is_alphanumeric(char c) {
    return is_alpha(c) || is_digit(c);
}

static Token make_token(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->current - 1;
    token.length = 1;
    token.line = lexer->line;
    return token;
}

static Token make_token_length(Lexer* lexer, TokenType type, size_t length) {
    Token token;
    token.type = type;
    token.start = lexer->current - length;
    token.length = length;
    token.line = lexer->line;
    return token;
}

static Token error_token(Lexer* lexer, const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = lexer->line;
    return token;
}

static char advance(Lexer* lexer) {
    lexer->current++;
    return lexer->current[-1];
}

static char peek(Lexer* lexer) {
    return *lexer->current;
}

static char peek_next(Lexer* lexer) {
    if (*lexer->current == '\0') return '\0';
    return lexer->current[1];
}

static int match(Lexer* lexer, char expected) {
    if (*lexer->current == expected) {
        lexer->current++;
        return 1;
    }
    return 0;
}

static void skip_whitespace(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                advance(lexer);
                break;
            case '/':
                // Verifica se é comentário de linha (//)
                if (peek_next(lexer) == '/') {
                    // Ignora até o fim da linha
                    while (peek(lexer) != '\n' && peek(lexer) != '\0') {
                        advance(lexer);
                    }
                } else {
                    // Não é comentário, retorna para processar como divisão
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType identifier_type(Lexer* lexer) {
    size_t length = lexer->current - lexer->start;
    
    switch (lexer->start[0]) {
        case 'l':
            if (length == 3 && memcmp(lexer->start + 1, "et", 2) == 0)
                return TOKEN_LET;
            break;
        case 'i':
            if (length == 2 && memcmp(lexer->start + 1, "f", 1) == 0)
                return TOKEN_IF;
            if (length == 3 && memcmp(lexer->start + 1, "nt", 2) == 0)
                return TOKEN_INT;
            break;
        case 'e':
            if (length == 4 && memcmp(lexer->start + 1, "lse", 3) == 0)
                return TOKEN_ELSE;
            break;
        case 'p':
            if (length == 5 && memcmp(lexer->start + 1, "rint", 4) == 0)
                return TOKEN_PRINT;
            break;
        case 'w':
            if (length == 5 && memcmp(lexer->start + 1, "hile", 4) == 0)
                return TOKEN_WHILE;
            break;
        case 'f':
            if (length == 2 && memcmp(lexer->start + 1, "n", 1) == 0)
                return TOKEN_FUNCTION;
            if (length == 3 && memcmp(lexer->start + 1, "or", 2) == 0)
                return TOKEN_FOR;
            if (length == 8 && memcmp(lexer->start + 1, "unction", 7) == 0)
                return TOKEN_FUNCTION;
            if (length == 5 && memcmp(lexer->start + 1, "alse", 4) == 0)
                return TOKEN_FALSE;
            if (length == 5 && memcmp(lexer->start + 1, "loat", 4) == 0)
                return TOKEN_FLOAT;
            break;
        case 'r':
            if (length == 6 && memcmp(lexer->start + 1, "eturn", 5) == 0)
                return TOKEN_RETURN;
            break;
        case 't':
            if (length == 4 && memcmp(lexer->start + 1, "rue", 3) == 0)
                return TOKEN_TRUE;
            break;
        case 'b':
            if (length == 4 && memcmp(lexer->start + 1, "ool", 3) == 0)
                return TOKEN_BOOL;
            break;
        case 's':
            if (length == 6 && memcmp(lexer->start + 1, "tring", 5) == 0)
                return TOKEN_STRING_TYPE;
            break;
        case 'v':
            if (length == 4 && memcmp(lexer->start + 1, "oid", 3) == 0)
                return TOKEN_VOID;
            break;
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Lexer* lexer) {
    while (is_alphanumeric(peek(lexer))) {
        advance(lexer);
    }
    return make_token_length(lexer, identifier_type(lexer), 
                            lexer->current - lexer->start);
}

static Token number(Lexer* lexer) {
    while (is_digit(peek(lexer))) {
        advance(lexer);
    }
    
    // Suporte para números decimais
    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        advance(lexer);
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
    }
    
    return make_token_length(lexer, TOKEN_NUMBER, 
                            lexer->current - lexer->start);
}

static Token string_token(Lexer* lexer) {
    while (peek(lexer) != '"' && peek(lexer) != '\0') {
        if (peek(lexer) == '\n') lexer->line++;
        advance(lexer);
    }
    
    if (peek(lexer) == '\0') {
        return error_token(lexer, "String não terminada");
    }
    
    // Consome a aspas de fechamento
    advance(lexer);
    return make_token_length(lexer, TOKEN_STRING, 
                            lexer->current - lexer->start);
}

Lexer* lexer_create(const char* source) {
    Lexer* lexer = (Lexer*)malloc(sizeof(Lexer));
    if (lexer == NULL) {
        return NULL;
    }
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    return lexer;
}

void lexer_destroy(Lexer* lexer) {
    if (lexer != NULL) {
        free(lexer);
    }
}

Token lexer_next_token(Lexer* lexer) {
    skip_whitespace(lexer);
    
    lexer->start = lexer->current;
    
    if (*lexer->current == '\0') {
        return make_token(lexer, TOKEN_EOF);
    }
    
    char c = advance(lexer);
    
    if (is_alpha(c)) {
        return identifier(lexer);
    }
    if (is_digit(c)) {
        return number(lexer);
    }
    
    switch (c) {
        case '(': return make_token(lexer, TOKEN_LPAREN);
        case ')': return make_token(lexer, TOKEN_RPAREN);
        case '{': return make_token(lexer, TOKEN_LBRACE);
        case '}': return make_token(lexer, TOKEN_RBRACE);
        case ';': return make_token(lexer, TOKEN_SEMICOLON);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case ':': return make_token(lexer, TOKEN_COLON);
        case '@': return make_token(lexer, TOKEN_AT);
        case '+': return make_token(lexer, TOKEN_PLUS);
        case '-': return make_token(lexer, TOKEN_MINUS);
        case '*': return make_token(lexer, TOKEN_MULTIPLY);
        case '/':
            // Comentários de linha já foram tratados em skip_whitespace
            // Se chegou aqui, é divisão
            return make_token(lexer, TOKEN_DIVIDE);
        case '!':
            if (match(lexer, '=')) {
                return make_token_length(lexer, TOKEN_NE, 2);
            }
            return make_token(lexer, TOKEN_NOT);
        case '=':
            if (match(lexer, '=')) {
                return make_token_length(lexer, TOKEN_EQ, 2);
            }
            return make_token(lexer, TOKEN_ASSIGN);
        case '>':
            if (match(lexer, '=')) {
                return make_token_length(lexer, TOKEN_GTE, 2);
            }
            return make_token(lexer, TOKEN_GT);
        case '<':
            if (match(lexer, '=')) {
                return make_token_length(lexer, TOKEN_LTE, 2);
            }
            return make_token(lexer, TOKEN_LT);
        case '&':
            if (match(lexer, '&')) {
                return make_token_length(lexer, TOKEN_AND, 2);
            }
            return error_token(lexer, "Esperado '&' após '&'");
        case '|':
            if (match(lexer, '|')) {
                return make_token_length(lexer, TOKEN_OR, 2);
            }
            return error_token(lexer, "Esperado '|' após '|'");
        case '"': return string_token(lexer);
        default:
            return error_token(lexer, "Caractere inesperado");
    }
}

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_LET: return "LET";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_PRINT: return "PRINT";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_FUNCTION: return "FUNCTION";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_INT: return "INT";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_BOOL: return "BOOL";
        case TOKEN_STRING_TYPE: return "STRING_TYPE";
        case TOKEN_VOID: return "VOID";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_MULTIPLY: return "MULTIPLY";
        case TOKEN_DIVIDE: return "DIVIDE";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_GT: return "GT";
        case TOKEN_LT: return "LT";
        case TOKEN_GTE: return "GTE";
        case TOKEN_LTE: return "LTE";
        case TOKEN_EQ: return "EQ";
        case TOKEN_NE: return "NE";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_COLON: return "COLON";
        case TOKEN_AT: return "AT";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void token_print(Token token) {
    printf("[Linha %d] %s", token.line, token_type_to_string(token.type));
    if (token.type == TOKEN_IDENTIFIER || 
        token.type == TOKEN_NUMBER || 
        token.type == TOKEN_STRING ||
        token.type == TOKEN_INT ||
        token.type == TOKEN_FLOAT ||
        token.type == TOKEN_BOOL ||
        token.type == TOKEN_STRING_TYPE ||
        token.type == TOKEN_VOID ||
        token.type == TOKEN_ERROR) {
        printf(" '%.*s'", (int)token.length, token.start);
    }
    printf("\n");
}

