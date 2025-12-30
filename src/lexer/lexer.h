#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

// Tipos de tokens
typedef enum {
    // Palavras-chave
    TOKEN_LET,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_PRINT,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_FUNCTION,
    TOKEN_RETURN,
    TOKEN_TRUE,
    TOKEN_FALSE,
    
    // Tipos
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_BOOL,
    TOKEN_STRING_TYPE,
    TOKEN_VOID,
    
    // Identificadores e literais
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    
    // Operadores
    TOKEN_PLUS,      // +
    TOKEN_MINUS,     // -
    TOKEN_MULTIPLY,  // *
    TOKEN_DIVIDE,    // /
    TOKEN_ASSIGN,    // =
    TOKEN_GT,        // >
    TOKEN_LT,        // <
    TOKEN_GTE,       // >=
    TOKEN_LTE,       // <=
    TOKEN_EQ,        // ==
    TOKEN_NE,        // !=
    TOKEN_AND,       // &&
    TOKEN_OR,        // ||
    TOKEN_NOT,       // !
    
    // Delimitadores
    TOKEN_LBRACE,    // {
    TOKEN_RBRACE,    // }
    TOKEN_LPAREN,    // (
    TOKEN_RPAREN,    // )
    
    // Especiais
    TOKEN_SEMICOLON, // ;
    TOKEN_COMMA,     // ,
    TOKEN_COLON,     // :
    TOKEN_AT,        // @ (para anotações)
    TOKEN_NEWLINE,   // \n
    TOKEN_EOF,       // Fim do arquivo
    TOKEN_ERROR      // Erro de tokenização
} TokenType;

// Estrutura de um token
typedef struct {
    TokenType type;
    const char* start;
    size_t length;
    int line;
} Token;

// Estrutura do lexer
typedef struct {
    const char* source;
    const char* start;
    const char* current;
    int line;
} Lexer;

// Funções do lexer
Lexer* lexer_create(const char* source);
void lexer_destroy(Lexer* lexer);
Token lexer_next_token(Lexer* lexer);
const char* token_type_to_string(TokenType type);
void token_print(Token token);

#endif // LEXER_H

