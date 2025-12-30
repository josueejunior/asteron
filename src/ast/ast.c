#include "ast.h"
#include "../utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== Sistema de Tipos ====================

TypeInfo* type_info_create(Type type, int is_inferred) {
    TypeInfo* info = (TypeInfo*)malloc(sizeof(TypeInfo));
    if (info == NULL) return NULL;
    info->type = type;
    info->is_inferred = is_inferred;
    return info;
}

void type_info_destroy(TypeInfo* info) {
    if (info != NULL) {
        free(info);
    }
}

const char* type_to_string(Type type) {
    switch (type) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
        case TYPE_STRING: return "string";
        case TYPE_VOID: return "void";
        case TYPE_UNKNOWN: return "unknown";
        case TYPE_ERROR: return "error";
        default: return "unknown";
    }
}

// ==================== Sistema de Anotações ====================

Annotation* annotation_create(AnnotationType type) {
    Annotation* annotation = (Annotation*)malloc(sizeof(Annotation));
    if (annotation == NULL) return NULL;
    annotation->type = type;
    annotation->dependencies = NULL;
    annotation->dep_count = 0;
    annotation->next = NULL;
    return annotation;
}

void annotation_destroy(Annotation* annotation) {
    if (annotation == NULL) return;
    
    if (annotation->dependencies != NULL) {
        for (size_t i = 0; i < annotation->dep_count; i++) {
            if (annotation->dependencies[i] != NULL) {
                free(annotation->dependencies[i]);
            }
        }
        free(annotation->dependencies);
    }
    
    // Destrói lista encadeada
    if (annotation->next != NULL) {
        annotation_destroy(annotation->next);
    }
    
    free(annotation);
}

void annotation_add_dependency(Annotation* annotation, const char* dep) {
    if (annotation == NULL || dep == NULL) return;
    
    // Verifica se já existe
    for (size_t i = 0; i < annotation->dep_count; i++) {
        if (annotation->dependencies[i] != NULL && 
            strcmp(annotation->dependencies[i], dep) == 0) {
            return;  // Já existe
        }
    }
    
    // Adiciona nova dependência
    size_t new_capacity = annotation->dep_count + 1;
    char** new_deps = (char**)realloc(annotation->dependencies, 
                                      sizeof(char*) * new_capacity);
    if (new_deps == NULL) return;
    
    annotation->dependencies = new_deps;
    annotation->dependencies[annotation->dep_count] = string_copy(dep, strlen(dep));
    annotation->dep_count++;
}

const char* annotation_type_to_string(AnnotationType type) {
    switch (type) {
        case ANNOTATION_PARALLEL: return "parallel";
        case ANNOTATION_DEPENDS: return "depends";
        case ANNOTATION_ASYNC: return "async";
        case ANNOTATION_CACHE: return "cache";
        default: return "unknown";
    }
}

// ==================== AST ====================

ASTNode* ast_create_node(ASTNodeType type) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (node == NULL) {
        return NULL;
    }
    node->type = type;
    node->inferred_type = NULL;
    node->annotations = NULL;
    node->is_readonly = 0;  // Inicialmente mutável
    node->start_pc = 0;
    node->end_pc = 0;
    memset(&node->as, 0, sizeof(node->as));
    return node;
}

void ast_destroy_node(ASTNode* node) {
    if (node == NULL) return;
    
    switch (node->type) {
        case AST_VARIABLE_DECLARATION:
            string_free(node->as.variable_decl.name);
            ast_destroy_node(node->as.variable_decl.value);
            if (node->as.variable_decl.type_info != NULL) {
                type_info_destroy(node->as.variable_decl.type_info);
            }
            // NÃO destruir anotações aqui, o cabeçalho do nó já faz isso
            break;
        case AST_BINARY_EXPRESSION:
            ast_destroy_node(node->as.binary_expr.left);
            ast_destroy_node(node->as.binary_expr.right);
            break;
        case AST_UNARY_EXPRESSION:
            ast_destroy_node(node->as.unary_expr.operand);
            break;
        case AST_LITERAL:
            if (node->as.literal.type == LIT_STRING) {
                string_free(node->as.literal.value.string);
            }
            break;
        case AST_IDENTIFIER:
            string_free(node->as.identifier.name);
            break;
        case AST_PRINT:
            ast_destroy_node(node->as.print_stmt.expression);
            // NÃO destruir anotações aqui, o cabeçalho do nó já faz isso
            break;
        case AST_IF_STATEMENT:
            ast_destroy_node(node->as.if_stmt.condition);
            ast_destroy_node(node->as.if_stmt.then_branch);
            if (node->as.if_stmt.else_branch) {
                ast_destroy_node(node->as.if_stmt.else_branch);
            }
            break;
        case AST_WHILE_STATEMENT:
            ast_destroy_node(node->as.while_stmt.condition);
            ast_destroy_node(node->as.while_stmt.body);
            break;
        case AST_FOR_STATEMENT:
            if (node->as.for_stmt.init) {
                ast_destroy_node(node->as.for_stmt.init);
            }
            if (node->as.for_stmt.condition) {
                ast_destroy_node(node->as.for_stmt.condition);
            }
            if (node->as.for_stmt.increment) {
                ast_destroy_node(node->as.for_stmt.increment);
            }
            ast_destroy_node(node->as.for_stmt.body);
            break;
        case AST_FUNCTION_DECLARATION:
            string_free(node->as.function_decl.name);
            for (size_t i = 0; i < node->as.function_decl.parameter_count; i++) {
                string_free(node->as.function_decl.parameters[i]);
                if (node->as.function_decl.param_types != NULL && 
                    node->as.function_decl.param_types[i] != NULL) {
                    type_info_destroy(node->as.function_decl.param_types[i]);
                }
            }
            free(node->as.function_decl.parameters);
            if (node->as.function_decl.param_types != NULL) {
                free(node->as.function_decl.param_types);
            }
            if (node->as.function_decl.return_type != NULL) {
                type_info_destroy(node->as.function_decl.return_type);
            }
            ast_destroy_node(node->as.function_decl.body);
            break;
        case AST_FUNCTION_CALL:
            string_free(node->as.function_call.name);
            for (size_t i = 0; i < node->as.function_call.argument_count; i++) {
                ast_destroy_node(node->as.function_call.arguments[i]);
            }
            free(node->as.function_call.arguments);
            break;
        case AST_RETURN:
            if (node->as.return_stmt.value) {
                ast_destroy_node(node->as.return_stmt.value);
            }
            break;
        case AST_BLOCK:
            for (size_t i = 0; i < node->as.block.count; i++) {
                ast_destroy_node(node->as.block.statements[i]);
            }
            free(node->as.block.statements);
            break;
        case AST_ASSIGNMENT:
            string_free(node->as.assignment.name);
            ast_destroy_node(node->as.assignment.value);
            break;
        case AST_PHI:
            string_free(node->as.phi.name);
            if (node->as.phi.versions != NULL) {
                free(node->as.phi.versions);
            }
            break;
        default:
            break;
    }
    
    if (node->inferred_type != NULL) {
        type_info_destroy(node->inferred_type);
    }
    
    if (node->annotations != NULL) {
        annotation_destroy(node->annotations);
    }
    
    free(node);
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void ast_print_node(ASTNode* node, int indent) {
    if (node == NULL) {
        print_indent(indent);
        printf("NULL\n");
        return;
    }
    
    switch (node->type) {
        case AST_VARIABLE_DECLARATION:
            print_indent(indent);
            printf("VariableDeclaration(%s_%d", node->as.variable_decl.name, node->as.variable_decl.ssa_version);
            if (node->as.variable_decl.type_info != NULL) {
                printf(": %s", type_to_string(node->as.variable_decl.type_info->type));
            }
            printf(")\n");
            ast_print_node(node->as.variable_decl.value, indent + 1);
            break;
        case AST_BINARY_EXPRESSION:
            print_indent(indent);
            printf("BinaryExpression(op: %d)\n", node->as.binary_expr.operator);
            ast_print_node(node->as.binary_expr.left, indent + 1);
            ast_print_node(node->as.binary_expr.right, indent + 1);
            break;
        case AST_UNARY_EXPRESSION:
            print_indent(indent);
            printf("UnaryExpression(op: %d)\n", node->as.unary_expr.operator);
            ast_print_node(node->as.unary_expr.operand, indent + 1);
            break;
        case AST_LITERAL:
            print_indent(indent);
            if (node->as.literal.type == LIT_NUMBER) {
                printf("Literal(number: %g)\n", node->as.literal.value.number);
            } else {
                printf("Literal(string: %s)\n", node->as.literal.value.string);
            }
            break;
        case AST_IDENTIFIER:
            print_indent(indent);
            printf("Identifier(%s_%d%s)\n", 
                   node->as.identifier.name, 
                   node->as.identifier.ssa_version,
                   node->as.identifier.is_last_use ? " [LAST]" : "");
            break;
        case AST_PRINT:
            print_indent(indent);
            printf("Print\n");
            ast_print_node(node->as.print_stmt.expression, indent + 1);
            break;
        case AST_IF_STATEMENT:
            print_indent(indent);
            printf("IfStatement\n");
            print_indent(indent + 1);
            printf("condition:\n");
            ast_print_node(node->as.if_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("then:\n");
            ast_print_node(node->as.if_stmt.then_branch, indent + 2);
            if (node->as.if_stmt.else_branch) {
                print_indent(indent + 1);
                printf("else:\n");
                ast_print_node(node->as.if_stmt.else_branch, indent + 2);
            }
            break;
        case AST_WHILE_STATEMENT:
            print_indent(indent);
            printf("WhileStatement\n");
            print_indent(indent + 1);
            printf("condition:\n");
            ast_print_node(node->as.while_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("body:\n");
            ast_print_node(node->as.while_stmt.body, indent + 2);
            break;
        case AST_FOR_STATEMENT:
            print_indent(indent);
            printf("ForStatement\n");
            if (node->as.for_stmt.init) {
                print_indent(indent + 1);
                printf("init:\n");
                ast_print_node(node->as.for_stmt.init, indent + 2);
            }
            if (node->as.for_stmt.condition) {
                print_indent(indent + 1);
                printf("condition:\n");
                ast_print_node(node->as.for_stmt.condition, indent + 2);
            }
            if (node->as.for_stmt.increment) {
                print_indent(indent + 1);
                printf("increment:\n");
                ast_print_node(node->as.for_stmt.increment, indent + 2);
            }
            print_indent(indent + 1);
            printf("body:\n");
            ast_print_node(node->as.for_stmt.body, indent + 2);
            break;
        case AST_FUNCTION_DECLARATION:
            print_indent(indent);
            printf("FunctionDeclaration(%s", node->as.function_decl.name);
            if (node->as.function_decl.return_type != NULL) {
                printf(" -> %s", type_to_string(node->as.function_decl.return_type->type));
            }
            printf(", %zu params)\n", node->as.function_decl.parameter_count);
            ast_print_node(node->as.function_decl.body, indent + 1);
            break;
        case AST_FUNCTION_CALL:
            print_indent(indent);
            printf("FunctionCall(%s, %zu args)\n", 
                   node->as.function_call.name, node->as.function_call.argument_count);
            for (size_t i = 0; i < node->as.function_call.argument_count; i++) {
                ast_print_node(node->as.function_call.arguments[i], indent + 1);
            }
            break;
        case AST_RETURN:
            print_indent(indent);
            printf("Return\n");
            if (node->as.return_stmt.value) {
                ast_print_node(node->as.return_stmt.value, indent + 1);
            }
            break;
        case AST_BLOCK:
            print_indent(indent);
            printf("Block(%zu statements)\n", node->as.block.count);
            for (size_t i = 0; i < node->as.block.count; i++) {
                ast_print_node(node->as.block.statements[i], indent + 1);
            }
            break;
        case AST_ASSIGNMENT:
            print_indent(indent);
            printf("Assignment(%s)\n", node->as.assignment.name);
            ast_print_node(node->as.assignment.value, indent + 1);
            break;
        case AST_PHI:
            print_indent(indent);
            printf("Phi(%s_%d = Φ(", node->as.phi.name, node->as.phi.result_version);
            for (size_t i = 0; i < node->as.phi.version_count; i++) {
                printf("%d%s", node->as.phi.versions[i], (i < node->as.phi.version_count - 1) ? ", " : "");
            }
            printf("))\n");
            break;
        default:
            print_indent(indent);
            printf("Unknown AST node type\n");
            break;
    }
}

ASTNode* ast_variable_declaration(char* name, ASTNode* value) {
    ASTNode* node = ast_create_node(AST_VARIABLE_DECLARATION);
    if (node == NULL) return NULL;
    node->as.variable_decl.name = name;
    node->as.variable_decl.value = value;
    node->as.variable_decl.type_info = NULL;  // Inferência
    node->as.variable_decl.annotations = NULL;
    node->as.variable_decl.ssa_version = -1;
    node->as.variable_decl.escapes = 1; // Por padrão, assume que escapa (seguro)
    return node;
}

ASTNode* ast_variable_declaration_with_type(char* name, ASTNode* value, TypeInfo* type_info) {
    ASTNode* node = ast_create_node(AST_VARIABLE_DECLARATION);
    if (node == NULL) return NULL;
    node->as.variable_decl.name = name;
    node->as.variable_decl.value = value;
    node->as.variable_decl.type_info = type_info;
    return node;
}

ASTNode* ast_binary_expression(ASTNode* left, int op, ASTNode* right) {
    ASTNode* node = ast_create_node(AST_BINARY_EXPRESSION);
    if (node == NULL) return NULL;
    node->as.binary_expr.left = left;
    node->as.binary_expr.operator = op;
    node->as.binary_expr.right = right;
    return node;
}

ASTNode* ast_literal_number(double value) {
    ASTNode* node = ast_create_node(AST_LITERAL);
    if (node == NULL) return NULL;
    node->as.literal.type = LIT_NUMBER;
    node->as.literal.value.number = value;
    return node;
}

ASTNode* ast_literal_string(char* value) {
    ASTNode* node = ast_create_node(AST_LITERAL);
    if (node == NULL) return NULL;
    node->as.literal.type = LIT_STRING;
    node->as.literal.value.string = value;
    return node;
}

ASTNode* ast_literal_bool(int value) {
    ASTNode* node = ast_create_node(AST_LITERAL);
    if (node == NULL) return NULL;
    node->as.literal.type = LIT_BOOL;
    node->as.literal.value.boolean = value;
    return node;
}

ASTNode* ast_identifier(char* name) {
    ASTNode* node = ast_create_node(AST_IDENTIFIER);
    if (node == NULL) return NULL;
    node->as.identifier.name = name;
    node->as.identifier.ssa_version = -1; // -1 significa "não transformado"
    node->as.identifier.is_last_use = 0;
    return node;
}

ASTNode* ast_print(ASTNode* expression) {
    ASTNode* node = ast_create_node(AST_PRINT);
    if (node == NULL) return NULL;
    node->as.print_stmt.expression = expression;
    node->as.print_stmt.annotations = NULL;
    return node;
}

ASTNode* ast_if_statement(ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch) {
    ASTNode* node = ast_create_node(AST_IF_STATEMENT);
    if (node == NULL) return NULL;
    node->as.if_stmt.condition = condition;
    node->as.if_stmt.then_branch = then_branch;
    node->as.if_stmt.else_branch = else_branch;
    return node;
}

ASTNode* ast_while_statement(ASTNode* condition, ASTNode* body) {
    ASTNode* node = ast_create_node(AST_WHILE_STATEMENT);
    if (node == NULL) return NULL;
    node->as.while_stmt.condition = condition;
    node->as.while_stmt.body = body;
    return node;
}

ASTNode* ast_for_statement(ASTNode* init, ASTNode* condition, ASTNode* increment, ASTNode* body) {
    ASTNode* node = ast_create_node(AST_FOR_STATEMENT);
    if (node == NULL) return NULL;
    node->as.for_stmt.init = init;
    node->as.for_stmt.condition = condition;
    node->as.for_stmt.increment = increment;
    node->as.for_stmt.body = body;
    return node;
}

ASTNode* ast_block(void) {
    ASTNode* node = ast_create_node(AST_BLOCK);
    if (node == NULL) return NULL;
    node->as.block.count = 0;
    node->as.block.capacity = 4;
    node->as.block.statements = (ASTNode**)malloc(sizeof(ASTNode*) * node->as.block.capacity);
    if (node->as.block.statements == NULL) {
        free(node);
        return NULL;
    }
    return node;
}

void ast_block_add_statement(ASTNode* block, ASTNode* statement) {
    if (block == NULL || block->type != AST_BLOCK) return;
    
    if (block->as.block.count >= block->as.block.capacity) {
        size_t new_capacity = block->as.block.capacity * 2;
        ASTNode** new_statements = (ASTNode**)realloc(
            block->as.block.statements,
            sizeof(ASTNode*) * new_capacity
        );
        if (new_statements == NULL) return;
        block->as.block.statements = new_statements;
        block->as.block.capacity = new_capacity;
    }
    
    block->as.block.statements[block->as.block.count++] = statement;
}

ASTNode* ast_function_declaration(char* name, char** parameters, size_t param_count, ASTNode* body) {
    ASTNode* node = ast_create_node(AST_FUNCTION_DECLARATION);
    if (node == NULL) return NULL;
    node->as.function_decl.name = name;
    node->as.function_decl.parameters = parameters;
    node->as.function_decl.param_types = NULL;
    node->as.function_decl.parameter_count = param_count;
    node->as.function_decl.return_type = NULL;
    node->as.function_decl.body = body;
    return node;
}

ASTNode* ast_function_declaration_with_types(char* name, char** parameters, TypeInfo** param_types, size_t param_count, TypeInfo* return_type, ASTNode* body) {
    ASTNode* node = ast_create_node(AST_FUNCTION_DECLARATION);
    if (node == NULL) return NULL;
    node->as.function_decl.name = name;
    node->as.function_decl.parameters = parameters;
    node->as.function_decl.param_types = param_types;
    node->as.function_decl.parameter_count = param_count;
    node->as.function_decl.return_type = return_type;
    node->as.function_decl.body = body;
    return node;
}

ASTNode* ast_function_call(char* name, ASTNode** arguments, size_t arg_count) {
    ASTNode* node = ast_create_node(AST_FUNCTION_CALL);
    if (node == NULL) return NULL;
    node->as.function_call.name = name;
    node->as.function_call.arguments = arguments;
    node->as.function_call.argument_count = arg_count;
    return node;
}

ASTNode* ast_return(ASTNode* value) {
    ASTNode* node = ast_create_node(AST_RETURN);
    if (node == NULL) return NULL;
    node->as.return_stmt.value = value;
    return node;
}

ASTNode* ast_phi(char* name) {
    ASTNode* node = ast_create_node(AST_PHI);
    if (node == NULL) return NULL;
    node->as.phi.name = string_copy(name, strlen(name));
    node->as.phi.versions = NULL;
    node->as.phi.version_count = 0;
    node->as.phi.result_version = -1;
    return node;
}

void ast_phi_add_version(ASTNode* phi_node, int version) {
    if (phi_node == NULL || phi_node->type != AST_PHI) return;
    phi_node->as.phi.versions = realloc(phi_node->as.phi.versions, sizeof(int) * (phi_node->as.phi.version_count + 1));
    phi_node->as.phi.versions[phi_node->as.phi.version_count++] = version;
}

ASTNode* ast_assignment(char* name, ASTNode* value) {
    ASTNode* node = ast_create_node(AST_ASSIGNMENT);
    if (node == NULL) return NULL;
    node->as.assignment.name = string_copy(name, strlen(name));
    node->as.assignment.value = value;
    return node;
}

void ast_mark_readonly(ASTNode* node) {
    if (node == NULL || node->is_readonly) return;
    
    node->is_readonly = 1;
    
    switch (node->type) {
        case AST_VARIABLE_DECLARATION:
            ast_mark_readonly(node->as.variable_decl.value);
            break;
        case AST_BINARY_EXPRESSION:
            ast_mark_readonly(node->as.binary_expr.left);
            ast_mark_readonly(node->as.binary_expr.right);
            break;
        case AST_UNARY_EXPRESSION:
            ast_mark_readonly(node->as.unary_expr.operand);
            break;
        case AST_PRINT:
            ast_mark_readonly(node->as.print_stmt.expression);
            break;
        case AST_IF_STATEMENT:
            ast_mark_readonly(node->as.if_stmt.condition);
            ast_mark_readonly(node->as.if_stmt.then_branch);
            ast_mark_readonly(node->as.if_stmt.else_branch);
            break;
        case AST_WHILE_STATEMENT:
            ast_mark_readonly(node->as.while_stmt.condition);
            ast_mark_readonly(node->as.while_stmt.body);
            break;
        case AST_FOR_STATEMENT:
            ast_mark_readonly(node->as.for_stmt.init);
            ast_mark_readonly(node->as.for_stmt.condition);
            ast_mark_readonly(node->as.for_stmt.increment);
            ast_mark_readonly(node->as.for_stmt.body);
            break;
        case AST_FUNCTION_DECLARATION:
            ast_mark_readonly(node->as.function_decl.body);
            break;
        case AST_FUNCTION_CALL:
            for (size_t i = 0; i < node->as.function_call.argument_count; i++) {
                ast_mark_readonly(node->as.function_call.arguments[i]);
            }
            break;
        case AST_RETURN:
            ast_mark_readonly(node->as.return_stmt.value);
            break;
        case AST_BLOCK:
            for (size_t i = 0; i < node->as.block.count; i++) {
                ast_mark_readonly(node->as.block.statements[i]);
            }
            break;
        case AST_ASSIGNMENT:
            ast_mark_readonly(node->as.assignment.value);
            break;
        case AST_PHI:
            break;
        case AST_EXPRESSION:
        case AST_LITERAL:
        case AST_IDENTIFIER:
            break;
    }
}

