// compiler.h -- definições gerais e estruturas para AST
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L // Adiciona a definição POSIX
#endif

#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Constantes ---

#define MAX_TOKEN_LEN 256
#define MAX_SYM 1024
#define MAX_FN_DEFS 256

// --- Tipos de Token ---

typedef enum {
    TOK_EOF, TOK_ID, TOK_NUMBER, TOK_STRING,
    TOK_LBRACK, TOK_RBRACK, TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE, TOK_EQ, TOK_COMMA, TOK_SEMI,
    TOK_FN, TOK_IF, TOK_ELSE, TOK_RETURN, TOK_SAY, TOK_HEAR,
    TOK_TYPE, TOK_OP, TOK_UNKNOWN, TOK_NEWLINE,
    TOK_OPERATOR, 
    // LITERAIS E LÓGICOS
    TOK_BOOL,
    TOK_TRUE,
    TOK_FALSE,
    TOK_AND, // and
    TOK_OR,  // or
    TOK_NOT, // not
    
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[MAX_TOKEN_LEN];
} Token;

/* Tipos de Nó da Abstract Syntax Tree (AST) */
typedef enum {
    N_PROGRAM,
    N_FN_DEF,
    N_VAR_DECL,
    N_VAR_ASSIGN,
    N_SAY,
    N_HEAR,
    N_IF,
    N_RETURN,
    N_EXPR_STMT, // Chamadas de função soltas (como comandos)
    N_STMT_LIST, // Para agrupar comandos/parâmetros

    // Expressões
    N_INT, N_FLOAT, N_STRING, N_BOOL,
    N_VAR,
    N_FN_CALL,
    N_ADD, N_SUB, N_MUL, N_DIV,
    N_GT, N_LT, N_EQ_CMP, N_NEQ, N_AND, N_OR, N_NOT,// OPERADOR UNÁRIO
    N_GTE, // Novo: Greater Than or Equal (>=)
    N_LTE // Novo: Less Than or Equal (<=)
} NodeKind;

// --- Estrutura do Nó da AST (CORRIGIDA) ---
typedef struct Node {
    NodeKind kind;
    char name[MAX_TOKEN_LEN]; // Nome da variável/função
    char text[MAX_TOKEN_LEN]; // Valor literal
    char typeName[MAX_TOKEN_LEN]; // Tipo inferido ou declarado
    
    // NOVO CAMPO: Tipo de retorno explícito (usado para return[tipo] valor)
    char explicitReturnType[MAX_TOKEN_LEN]; 
    
    struct Node *left;  // Expressão / Parâmetros
    struct Node *mid;   // Corpo da função / Bloco ELSE
    struct Node *right; // Próximo na lista / Bloco THEN
} Node;

// --- Prototipos da AST (CORRIGIDOS) ---

// Funções de utilidade para a AST
Node *make_node(NodeKind kind, const char *name, const char *text, Node *left, Node *mid, Node *right);

// Prototipo para criar N_RETURN sem tipo explícito
Node *make_return_node(Node *expr); 

// NOVO PROTOTIPO: Para criar N_RETURN com tipo explícito
Node *make_return_node_with_type(const char *typeName, Node *expr);


/* Variáveis Globais para a AST (Armazenadas pelo parser) */
extern Node *fn_defs[MAX_FN_DEFS];
extern int fnDefCount;
extern Node *global_stmts[MAX_FN_DEFS];
extern int globalStmtCount;

// Prototipos da Geração de Código
void generate_code(const char *out_c, Node *program_root);

// Lexer (Prototipos existentes)
void parse_all();
extern Token curtok;
Token next_token();
void lexer_init_from_string(const char* s);

#endif