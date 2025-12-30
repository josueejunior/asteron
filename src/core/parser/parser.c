#include "parser.h"
#include "../../utils/utils.h"
#include "../lexer/lexer.h"
#include "../ast/ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Forward declarations
static TypeInfo* parse_type(Parser* parser);
static Annotation* parse_annotation(Parser* parser);
static ASTNode* parse_logical_or(Parser* parser);
static ASTNode* parse_logical_and(Parser* parser);
static ASTNode* parse_equality(Parser* parser);
static ASTNode* parse_comparison(Parser* parser);
static ASTNode* parse_additive(Parser* parser);

static void advance(Parser* parser) {
    parser->previous = parser->current;
    parser->current = lexer_next_token(parser->lexer);
    
    if (parser->current.type == TOKEN_ERROR) {
        parser->had_error = 1;
    }
}

static int check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

static int match(Parser* parser, TokenType type) {
    if (check(parser, type)) {
        advance(parser);
        return 1;
    }
    return 0;
}

static void consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "%s (encontrado: %s)", message, 
             token_type_to_string(parser->current.type));
    error_report(parser->current.line, error_msg);
    parser->had_error = 1;
}

static char* get_token_string(Token token) {
    return string_copy(token.start, token.length);
}

static TypeInfo* parse_type(Parser* parser) {
    Type type = TYPE_UNKNOWN;
    if (match(parser, TOKEN_INT)) type = TYPE_INT;
    else if (match(parser, TOKEN_FLOAT)) type = TYPE_FLOAT;
    else if (match(parser, TOKEN_BOOL)) type = TYPE_BOOL;
    else if (match(parser, TOKEN_STRING_TYPE)) type = TYPE_STRING;
    else if (match(parser, TOKEN_VOID)) type = TYPE_VOID;
    else return NULL;
    return type_info_create(type, 0);
}

static Annotation* parse_annotation(Parser* parser) {
    Annotation* annotation = NULL;
    
    /* Verifica tokens de anotação diretos (do lexer) */
    if (match(parser, TOKEN_ANNOT_PURE)) {
        annotation = annotation_create(ANNOTATION_PURE);
    } else if (match(parser, TOKEN_ANNOT_PARALLEL)) {
        annotation = annotation_create(ANNOTATION_PARALLEL);
    } else if (match(parser, TOKEN_ANNOT_ASYNC)) {
        annotation = annotation_create(ANNOTATION_ASYNC);
    } else if (match(parser, TOKEN_ANNOT_MEMOIZE)) {
        annotation = annotation_create(ANNOTATION_MEMOIZE);
    } else if (match(parser, TOKEN_ANNOT_LAZY)) {
        annotation = annotation_create(ANNOTATION_LAZY);
    } else if (match(parser, TOKEN_ANNOT_HOT)) {
        annotation = annotation_create(ANNOTATION_HOT);
    } else if (match(parser, TOKEN_ANNOT_INLINE)) {
        annotation = annotation_create(ANNOTATION_INLINE);
    } else if (match(parser, TOKEN_ANNOT_NOOPT)) {
        annotation = annotation_create(ANNOTATION_NOOPT);
    }
    /* Fallback para @ + identificador (estilo antigo) */
    else if (match(parser, TOKEN_AT)) {
        if (!match(parser, TOKEN_IDENTIFIER)) {
            error_report(parser->current.line, "Esperado nome de anotação após '@'");
            parser->had_error = 1; return NULL;
        }
        char* name = get_token_string(parser->previous);
        if (strcmp(name, "parallel") == 0) annotation = annotation_create(ANNOTATION_PARALLEL);
        else if (strcmp(name, "depends") == 0) {
            annotation = annotation_create(ANNOTATION_DEPENDS);
            if (match(parser, TOKEN_LPAREN)) {
                if (!check(parser, TOKEN_RPAREN)) {
                    consume(parser, TOKEN_IDENTIFIER, "Esperado nome de dependência");
                    char* dep = get_token_string(parser->previous);
                    annotation_add_dependency(annotation, dep); string_free(dep);
                    while (match(parser, TOKEN_COMMA)) {
                        consume(parser, TOKEN_IDENTIFIER, "Esperado nome de dependência após ','");
                        dep = get_token_string(parser->previous);
                        annotation_add_dependency(annotation, dep); string_free(dep);
                    }
                }
                consume(parser, TOKEN_RPAREN, "Esperado ')' após lista de dependências");
            }
        } else if (strcmp(name, "async") == 0) annotation = annotation_create(ANNOTATION_ASYNC);
        else if (strcmp(name, "cache") == 0) annotation = annotation_create(ANNOTATION_CACHE);
        else if (strcmp(name, "pure") == 0) annotation = annotation_create(ANNOTATION_PURE);
        else if (strcmp(name, "memoize") == 0) annotation = annotation_create(ANNOTATION_MEMOIZE);
        else if (strcmp(name, "lazy") == 0) annotation = annotation_create(ANNOTATION_LAZY);
        else if (strcmp(name, "hot") == 0) annotation = annotation_create(ANNOTATION_HOT);
        else if (strcmp(name, "inline") == 0) annotation = annotation_create(ANNOTATION_INLINE);
        else if (strcmp(name, "noopt") == 0) annotation = annotation_create(ANNOTATION_NOOPT);
        string_free(name);
    }
    
    return annotation;
}

static double get_token_number(Token token) {
    char* str = string_copy(token.start, token.length);
    double value = strtod(str, NULL);
    string_free(str);
    return value;
}

Parser* parser_create(Lexer* lexer) {
    Parser* parser = (Parser*)malloc(sizeof(Parser));
    if (parser == NULL) return NULL;
    parser->lexer = lexer;
    parser->had_error = 0;
    advance(parser);
    return parser;
}

void parser_destroy(Parser* parser) {
    if (parser != NULL) free(parser);
}

int parser_had_error(Parser* parser) {
    return parser->had_error;
}

ASTNode* parse_factor(Parser* parser) {
    if (match(parser, TOKEN_NOT)) {
        ASTNode* operand = parse_factor(parser);
        if (operand == NULL) return NULL;
        ASTNode* node = ast_create_node(AST_UNARY_EXPRESSION);
        if (node == NULL) { ast_destroy_node(operand); return NULL; }
        node->as.unary_expr.operator = TOKEN_NOT;
        node->as.unary_expr.operand = operand;
        return node;
    }
    if (match(parser, TOKEN_NUMBER)) return ast_literal_number(get_token_number(parser->previous));
    if (match(parser, TOKEN_STRING)) {
        char* raw_str = get_token_string(parser->previous);
        /* Usa string_unescape para processar escape sequences */
        char* str = string_unescape(raw_str, raw_str ? strlen(raw_str) : 0);
        string_free(raw_str);
        return ast_literal_string(str);
    }
    if (match(parser, TOKEN_TRUE)) return ast_literal_bool(1);
    if (match(parser, TOKEN_FALSE)) return ast_literal_bool(0);
    if (match(parser, TOKEN_IDENTIFIER)) {
        char* name = get_token_string(parser->previous);
        if (match(parser, TOKEN_LPAREN)) {
            ASTNode** args = NULL; size_t count = 0; size_t cap = 4;
            if (!check(parser, TOKEN_RPAREN)) {
                args = (ASTNode**)malloc(sizeof(ASTNode*) * cap);
                ASTNode* arg = parse_expression(parser);
                args[count++] = arg;
                while (match(parser, TOKEN_COMMA)) {
                    if (count >= cap) { cap *= 2; args = realloc(args, sizeof(ASTNode*) * cap); }
                    args[count++] = parse_expression(parser);
                }
            }
            consume(parser, TOKEN_RPAREN, "Esperado ')' após argumentos");
            return ast_function_call(name, args, count);
        }
        return ast_identifier(name);
    }
    if (match(parser, TOKEN_LPAREN)) {
        ASTNode* expr = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Esperado ')' após expressão");
        return expr;
    }
    return NULL;
}

ASTNode* parse_term(Parser* parser) {
    ASTNode* expr = parse_factor(parser);
    if (expr == NULL) return NULL;
    while (match(parser, TOKEN_MULTIPLY) || match(parser, TOKEN_DIVIDE)) {
        int op = parser->previous.type;
        ASTNode* right = parse_factor(parser);
        expr = ast_binary_expression(expr, op, right);
    }
    return expr;
}

static ASTNode* parse_additive(Parser* parser) {
    ASTNode* expr = parse_term(parser);
    if (expr == NULL) return NULL;
    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
        int op = parser->previous.type;
        ASTNode* right = parse_term(parser);
        expr = ast_binary_expression(expr, op, right);
    }
    return expr;
}

static ASTNode* parse_comparison(Parser* parser) {
    ASTNode* expr = parse_additive(parser);
    if (expr == NULL) return NULL;
    while (match(parser, TOKEN_GT) || match(parser, TOKEN_LT) || match(parser, TOKEN_GTE) || match(parser, TOKEN_LTE)) {
        int op = parser->previous.type;
        ASTNode* right = parse_additive(parser);
        expr = ast_binary_expression(expr, op, right);
    }
    return expr;
}

static ASTNode* parse_equality(Parser* parser) {
    ASTNode* expr = parse_comparison(parser);
    if (expr == NULL) return NULL;
    while (match(parser, TOKEN_EQ) || match(parser, TOKEN_NE)) {
        int op = parser->previous.type;
        ASTNode* right = parse_comparison(parser);
        expr = ast_binary_expression(expr, op, right);
    }
    return expr;
}

static ASTNode* parse_logical_and(Parser* parser) {
    ASTNode* expr = parse_equality(parser);
    if (expr == NULL) return NULL;
    while (match(parser, TOKEN_AND)) {
        int op = parser->previous.type;
        ASTNode* right = parse_equality(parser);
        expr = ast_binary_expression(expr, op, right);
    }
    return expr;
}

static ASTNode* parse_logical_or(Parser* parser) {
    ASTNode* expr = parse_logical_and(parser);
    if (expr == NULL) return NULL;
    while (match(parser, TOKEN_OR)) {
        int op = parser->previous.type;
        ASTNode* right = parse_logical_and(parser);
        expr = ast_binary_expression(expr, op, right);
    }
    return expr;
}

ASTNode* parse_expression(Parser* parser) {
    return parse_logical_or(parser);
}

/* Helper para verificar se o token atual é uma anotação */
static int is_annotation_token(Parser* parser) {
    TokenType t = parser->current.type;
    return t == TOKEN_AT || 
           t == TOKEN_ANNOT_PURE ||
           t == TOKEN_ANNOT_PARALLEL ||
           t == TOKEN_ANNOT_ASYNC ||
           t == TOKEN_ANNOT_MEMOIZE ||
           t == TOKEN_ANNOT_LAZY ||
           t == TOKEN_ANNOT_HOT ||
           t == TOKEN_ANNOT_INLINE ||
           t == TOKEN_ANNOT_NOOPT;
}

ASTNode* parse_statement(Parser* parser) {
    Annotation* annotations = NULL;
    Annotation* last_annotation = NULL;
    while (is_annotation_token(parser)) {
        Annotation* a = parse_annotation(parser);
        if (annotations == NULL) { annotations = a; last_annotation = a; }
        else { last_annotation->next = a; last_annotation = a; }
    }

    if (check(parser, TOKEN_IDENTIFIER)) {
        Token id_token = parser->current;
        Lexer save = *parser->lexer;
        advance(parser);
        if (match(parser, TOKEN_ASSIGN)) {
            char* name = string_copy(id_token.start, id_token.length);
            ASTNode* val = parse_expression(parser);
            match(parser, TOKEN_SEMICOLON);
            return ast_assignment(name, val);
        }
        *parser->lexer = save; parser->current = id_token;
    }

    if (match(parser, TOKEN_FUNCTION)) {
        consume(parser, TOKEN_IDENTIFIER, "Esperado nome de função");
        char* name = get_token_string(parser->previous);
        consume(parser, TOKEN_LPAREN, "Esperado '('");
        char** params = NULL; size_t count = 0;
        if (!check(parser, TOKEN_RPAREN)) {
            params = malloc(sizeof(char*) * 4);
            consume(parser, TOKEN_IDENTIFIER, "Esperado parâmetro");
            params[count++] = get_token_string(parser->previous);
            while (match(parser, TOKEN_COMMA)) {
                consume(parser, TOKEN_IDENTIFIER, "Esperado parâmetro");
                params[count++] = get_token_string(parser->previous);
            }
        }
        consume(parser, TOKEN_RPAREN, "Esperado ')'");
        ASTNode* body = parse_statement(parser);
        return ast_function_declaration(name, params, count, body);
    }

    if (match(parser, TOKEN_RETURN)) {
        ASTNode* val = NULL;
        // Se não há ponto e vírgula, tenta parsear expressão
        if (!check(parser, TOKEN_SEMICOLON) && !check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
            val = parse_expression(parser);
        }
        // Ponto e vírgula é opcional
        match(parser, TOKEN_SEMICOLON);
        return ast_return(val);
    }

    if (match(parser, TOKEN_LET)) {
        consume(parser, TOKEN_IDENTIFIER, "Esperado nome");
        char* name = get_token_string(parser->previous);
        TypeInfo* type = NULL;
        if (match(parser, TOKEN_COLON)) type = parse_type(parser);
        consume(parser, TOKEN_ASSIGN, "Esperado '='");
        ASTNode* val = parse_expression(parser);
        // Ponto e vírgula é opcional - aceita se presente, mas não requer
        match(parser, TOKEN_SEMICOLON);
        ASTNode* node = ast_variable_declaration_with_type(name, val, type);
        if (node) node->annotations = annotations;
        return node;
    }

    if (match(parser, TOKEN_PRINT)) {
        consume(parser, TOKEN_LPAREN, "Esperado '('");
        ASTNode* expr = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Esperado ')'");
        // Ponto e vírgula é opcional - aceita se presente, mas não requer
        match(parser, TOKEN_SEMICOLON);
        ASTNode* node = ast_print(expr);
        if (node) node->annotations = annotations;
        return node;
    }

    if (match(parser, TOKEN_IF)) {
        consume(parser, TOKEN_LPAREN, "Esperado '('");
        ASTNode* cond = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Esperado ')'");
        ASTNode* then_b = parse_statement(parser);
        ASTNode* else_b = match(parser, TOKEN_ELSE) ? parse_statement(parser) : NULL;
        return ast_if_statement(cond, then_b, else_b);
    }

    if (match(parser, TOKEN_WHILE)) {
        consume(parser, TOKEN_LPAREN, "Esperado '('");
        ASTNode* cond = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "Esperado ')'");
        return ast_while_statement(cond, parse_statement(parser));
    }

    if (match(parser, TOKEN_FOR)) {
        consume(parser, TOKEN_LPAREN, "Esperado '('");
        ASTNode* init = !check(parser, TOKEN_SEMICOLON) ? parse_statement(parser) : NULL;
        if (init == NULL) match(parser, TOKEN_SEMICOLON);
        ASTNode* cond = !check(parser, TOKEN_SEMICOLON) ? parse_expression(parser) : NULL;
        consume(parser, TOKEN_SEMICOLON, "Esperado ';'");
        ASTNode* inc = !check(parser, TOKEN_RPAREN) ? (check(parser, TOKEN_LET) ? parse_statement(parser) : parse_expression(parser)) : NULL;
        consume(parser, TOKEN_RPAREN, "Esperado ')'");
        return ast_for_statement(init, cond, inc, parse_statement(parser));
    }

    if (match(parser, TOKEN_LBRACE)) {
        ASTNode* block = ast_block();
        while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
            // Pula ponto e vírgula solto no início de declaração
            if (check(parser, TOKEN_SEMICOLON)) {
                advance(parser);
                continue;
            }
            ASTNode* s = parse_statement(parser);
            if (s) ast_block_add_statement(block, s);
            else if (parser->had_error) {
                // Recuperação de erro: pula até próximo ponto e vírgula ou chave de fechamento
                while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF) && !check(parser, TOKEN_SEMICOLON)) advance(parser);
                match(parser, TOKEN_SEMICOLON); parser->had_error = 0;
            } else break;
        }
        consume(parser, TOKEN_RBRACE, "Esperado '}'");
        return block;
    }

    /* import math
     * import math.sin
     * from math import sin, cos
     * from math import sin as seno
     */
    if (match(parser, TOKEN_IMPORT)) {
        consume(parser, TOKEN_IDENTIFIER, "Esperado nome do módulo");
        char* module_name = get_token_string(parser->previous);
        
        char** symbols = NULL;
        size_t symbol_count = 0;
        
        /* import math.sin */
        if (match(parser, TOKEN_DOT)) {
            consume(parser, TOKEN_IDENTIFIER, "Esperado nome do símbolo");
            symbols = malloc(sizeof(char*));
            symbols[0] = get_token_string(parser->previous);
            symbol_count = 1;
        }
        
        match(parser, TOKEN_SEMICOLON);
        return ast_import(module_name, symbols, symbol_count, NULL);
    }
    
    if (match(parser, TOKEN_FROM)) {
        consume(parser, TOKEN_IDENTIFIER, "Esperado nome do módulo");
        char* module_name = get_token_string(parser->previous);
        consume(parser, TOKEN_IMPORT, "Esperado 'import'");
        
        /* Coleta símbolos */
        size_t capacity = 8;
        char** symbols = malloc(sizeof(char*) * capacity);
        char** aliases = malloc(sizeof(char*) * capacity);
        size_t count = 0;
        
        do {
            consume(parser, TOKEN_IDENTIFIER, "Esperado nome do símbolo");
            if (count >= capacity) {
                capacity *= 2;
                symbols = realloc(symbols, sizeof(char*) * capacity);
                aliases = realloc(aliases, sizeof(char*) * capacity);
            }
            symbols[count] = get_token_string(parser->previous);
            aliases[count] = NULL;
            
            /* Check for 'as' alias */
            if (match(parser, TOKEN_AS)) {
                consume(parser, TOKEN_IDENTIFIER, "Esperado alias");
                aliases[count] = get_token_string(parser->previous);
            }
            count++;
        } while (match(parser, TOKEN_COMMA));
        
        match(parser, TOKEN_SEMICOLON);
        return ast_import(module_name, symbols, count, aliases);
    }

    ASTNode* expr = parse_expression(parser);
    if (expr) { match(parser, TOKEN_SEMICOLON); return expr; }
    
    error_report(parser->current.line, "Esperado declaração, atribuição, print, if, while, for, bloco ou expressão");
    parser->had_error = 1; return NULL;
}

ASTNode* parser_parse(Parser* parser) {
    ASTNode* block = ast_block();
    while (!check(parser, TOKEN_EOF)) {
        ASTNode* s = parse_statement(parser);
        if (s) ast_block_add_statement(block, s);
        else break;
    }
    return block;
}
