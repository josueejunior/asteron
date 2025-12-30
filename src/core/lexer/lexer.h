/*
 * Asteron Runtime - (C) 2024 Asteron Contributors
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
    TOKEN_IMPORT,
    TOKEN_FROM,
    TOKEN_AS,
    TOKEN_EXPORT,
    TOKEN_MODULE,
    
    // Reactive keywords
    TOKEN_REACTIVE,     // reactive (modificador de let)
    TOKEN_WHEN,         // when (gatilho)
    TOKEN_CONTEXT,      // context (contexto vivo)
    TOKEN_DERIVE,       // derive (valor derivado)
    TOKEN_ON,           // on (evento)
    TOKEN_EMIT,         // emit (emissão de evento)
    TOKEN_WATCH,        // watch (observador)
    
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
    TOKEN_DOT,       // .
    TOKEN_AT,        // @ (para anotações)
    TOKEN_NEWLINE,   // \n
    TOKEN_EOF,       // Fim do arquivo
    TOKEN_ERROR,     // Erro de tokenização
    
    // Anotações para Grafo Declarativo
    TOKEN_ANNOT_PURE,      // @pure - função pura (memoizável)
    TOKEN_ANNOT_PARALLEL,  // @parallel - bloco paralelizável
    TOKEN_ANNOT_ASYNC,     // @async - operação assíncrona
    TOKEN_ANNOT_MEMOIZE,   // @memoize - resultado cacheado
    TOKEN_ANNOT_LAZY,      // @lazy - avaliação preguiçosa
    TOKEN_ANNOT_HOT,       // @hot - marca como hot path para JIT
    TOKEN_ANNOT_INLINE,    // @inline - sugestão de inline
    TOKEN_ANNOT_NOOPT      // @noopt - não otimizar
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

