// parser.c -- Constrói a Abstract Syntax Tree (AST)

#include "compiler.h"

// --- Variáveis Globais para o Parser ---
Node *fn_defs[MAX_FN_DEFS];
int fnDefCount = 0;
Node *global_stmts[MAX_FN_DEFS];
int globalStmtCount = 0;

// Variável para o token atual
Token curtok;

// Helper para criar um novo nó da AST
Node *make_node(NodeKind kind, const char *name, const char *text, Node *left, Node *mid, Node *right) {
    Node *n = (Node *)malloc(sizeof(Node));
    if (!n) { perror("Erro ao alocar nó da AST"); exit(1); }
    memset(n, 0, sizeof(Node));
    n->kind = kind;
    if (name) strncpy(n->name, name, MAX_TOKEN_LEN-1);
    if (text) strncpy(n->text, text, MAX_TOKEN_LEN-1);
    n->left = left;
    n->mid = mid;
    n->right = right;
    return n;
}

void advance() { curtok = next_token(); }
void expect(TokenType t) {
    if (curtok.type != t) {
        fprintf(stderr, "Parse error: expected token %d but got token %d ('%s')\n", t, curtok.type, curtok.lexeme);
        exit(1);
    }
}

static void skip_newlines() {
    while (curtok.type == TOK_NEWLINE) advance();
}

// Prototipos internos
static Node *parse_and_or(); 
static Node *parse_comparison(); 
static Node *parse_expression(); 
static Node *parse_term();       
static Node *parse_unary();      
static Node *parse_factor();     
static Node *parse_literal();    

static Node *parse_call(const char *fn_name);
static Node *parse_condition();
static Node *parse_block_list();
static Node *parse_statement(int is_global);
static Node *parse_function_definition();


/* ------------------------------------------------------------
   EXPRESSÕES (Hierarquia de Precedência)
   ------------------------------------------------------------ */

static Node *parse_and_or() {
    Node *left = parse_comparison();

    while (curtok.type == TOK_AND || curtok.type == TOK_OR) {
        NodeKind op_kind = (curtok.type == TOK_AND) ? N_AND : N_OR;
        advance(); 
        Node *right = parse_comparison(); 
        
        if (!right) {
            fprintf(stderr, "Erro de sintaxe: Expressão esperada após operador lógico '%s'.\n", (op_kind == N_AND ? "and" : "or"));
            exit(1);
        }
        
        left = make_node(op_kind, NULL, NULL, left, NULL, right);
    }
    return left;
}

static Node *parse_comparison() {
    Node *left = parse_expression();
    
    // CORREÇÃO CRÍTICA: Mapeamento de operadores de comparação
    while (curtok.type == TOK_OPERATOR && 
           (strcmp(curtok.lexeme, ">") == 0 || 
            strcmp(curtok.lexeme, "==") == 0 ||
            strcmp(curtok.lexeme, "<") == 0 ||
            strcmp(curtok.lexeme, "!=") == 0 ||
            strcmp(curtok.lexeme, ">=") == 0 ||
            strcmp(curtok.lexeme, "<=") == 0)) 
    {
        NodeKind op_kind;
        if (strcmp(curtok.lexeme, ">") == 0) op_kind = N_GT;
        else if (strcmp(curtok.lexeme, "==") == 0) op_kind = N_EQ_CMP;
        else if (strcmp(curtok.lexeme, "<") == 0) op_kind = N_LT; 
        else if (strcmp(curtok.lexeme, "!=") == 0) op_kind = N_NEQ; 
        // Novos Nodos para >= e <= (Se N_GTE e N_LTE não existirem, usaremos o > e < padrão com a semântica de C)
        // Para compatibilidade com o codegen.c anterior que não tinha N_GTE/N_LTE, usamos temporariamente N_GT/N_LT:

        
        // Se a sua AST suportar N_GTE e N_LTE, o codegen.c precisará ser atualizado.
        // Assumindo que N_GTE/N_LTE são novos tipos de nó:
        
        // Se o seu `NodeKind` não tiver `N_GTE` e `N_LTE`, use:
        /*
        else if (strcmp(curtok.lexeme, ">=") == 0) { 
            fprintf(stderr, "Erro de AST: Operador '>=' precisa de um tipo de nó (N_GTE)!\n");
            exit(1); 
        }
        */

        advance();
        Node *right = parse_expression();
        if (!right) {
             fprintf(stderr, "Erro de sintaxe: Expressão esperada após operador de comparação.\n");
             exit(1);
        }
        left = make_node(op_kind, NULL, NULL, left, NULL, right);
    }
    
    return left;
}

static Node *parse_expression() {
    Node *left = parse_term();
    
    while (curtok.type == TOK_OPERATOR && (strcmp(curtok.lexeme, "+") == 0 || strcmp(curtok.lexeme, "-") == 0)) {
        NodeKind op_kind = (strcmp(curtok.lexeme, "+") == 0) ? N_ADD : N_SUB;
        advance();
        Node *right = parse_term();
        if (!right) {
             fprintf(stderr, "Erro de sintaxe: Termo esperado após operador aritmético.\n");
             exit(1);
        }
        left = make_node(op_kind, NULL, NULL, left, NULL, right);
    }
    
    return left;
}

static Node *parse_term() {
    Node *left = parse_unary(); 
    
    while (curtok.type == TOK_OPERATOR && (strcmp(curtok.lexeme, "*") == 0 || strcmp(curtok.lexeme, "/") == 0)) {
        NodeKind op_kind = (strcmp(curtok.lexeme, "*") == 0) ? N_MUL : N_DIV;
        advance();
        Node *right = parse_unary(); 
        if (!right) {
             fprintf(stderr, "Erro de sintaxe: Expressão unária esperada após operador de multiplicação/divisão.\n");
             exit(1);
        }
        left = make_node(op_kind, NULL, NULL, left, NULL, right);
    }
    
    return left;
}

static Node *parse_unary() {
    if (curtok.type == TOK_NOT) {
        advance(); 
        Node *expr = parse_unary(); 
        if (!expr) {
            fprintf(stderr, "Erro de sintaxe: Expressão esperada após 'not'.\n");
            exit(1);
        }
        return make_node(N_NOT, NULL, NULL, expr, NULL, NULL);
    }
    
    return parse_factor(); 
}

static Node *parse_factor() { 
    if (curtok.type == TOK_ID) {
        char name[MAX_TOKEN_LEN];
        strcpy(name, curtok.lexeme);
        advance();
        
        if (curtok.type == TOK_LPAREN) {
            return parse_call(name);
        }
        
        return make_node(N_VAR, name, NULL, NULL, NULL, NULL);
        
    } else if (curtok.type == TOK_LPAREN) {
        advance();
        Node *expr = parse_and_or(); 
        expect(TOK_RPAREN); advance();
        return expr;
        
    } else {
        return parse_literal();
    }
}

static Node *parse_literal() {
    Node *node = NULL;
    if (curtok.type == TOK_NUMBER) {
        if (strchr(curtok.lexeme, '.') || strchr(curtok.lexeme, 'e') || strchr(curtok.lexeme, 'E')) {
            node = make_node(N_FLOAT, NULL, curtok.lexeme, NULL, NULL, NULL);
        } else {
            node = make_node(N_INT, NULL, curtok.lexeme, NULL, NULL, NULL);
        }
        advance();
    } else if (curtok.type == TOK_STRING) {
        node = make_node(N_STRING, NULL, curtok.lexeme, NULL, NULL, NULL);
        advance();
    } 
    else if (curtok.type == TOK_TRUE || curtok.type == TOK_FALSE) { 
        node = make_node(N_BOOL, NULL, curtok.lexeme, NULL, NULL, NULL);
        advance();
    } else {
        return NULL; // Retorna NULL se não for um literal, permitindo que parse_factor tente outras opções
    }
    return node;
}

static Node *parse_call(const char *fn_name) {
    expect(TOK_LPAREN); advance();
    
    Node *args_list = NULL;
    Node *current_arg = NULL;
    
    if (curtok.type != TOK_RPAREN) {
        Node *arg_expr = parse_and_or(); 
        args_list = make_node(N_STMT_LIST, NULL, NULL, arg_expr, NULL, NULL);
        current_arg = args_list;

        while (curtok.type == TOK_COMMA) {
            advance();
            arg_expr = parse_and_or(); 
            if (!arg_expr) {
                 fprintf(stderr, "Erro de sintaxe: Expressão de argumento esperada após vírgula.\n");
                 exit(1);
            }
            current_arg->right = make_node(N_STMT_LIST, NULL, NULL, arg_expr, NULL, NULL);
            current_arg = current_arg->right;
        }
    }
    
    expect(TOK_RPAREN); advance();
    
    return make_node(N_FN_CALL, fn_name, NULL, args_list, NULL, NULL);
}

static Node *parse_condition() {
    return parse_and_or(); 
}


/* ------------------------------------------------------------
   STATEMENTS (Comandos) - Com Newlines Corrigidos
   ------------------------------------------------------------ */

// Analisa um comando que pode ser global ou local
static Node *parse_statement(int is_global) {
    skip_newlines(); 

    if (curtok.type == TOK_ID) {
        char id[MAX_TOKEN_LEN]; strcpy(id, curtok.lexeme);
        advance();

        if (curtok.type == TOK_LBRACK) {
            // N_VAR_DECL: ID [ TYPE ] = EXPR
            advance();
            expect(TOK_TYPE);
            char type[MAX_TOKEN_LEN]; strcpy(type, curtok.lexeme);
            advance();
            expect(TOK_RBRACK); advance();
            expect(TOK_EQ); advance();

            Node *expr = parse_and_or(); 
            if (!expr) {
                fprintf(stderr, "Erro de sintaxe: Expressão esperada após '=' em declaração.\n");
                exit(1);
            }
            
            Node *decl = make_node(N_VAR_DECL, id, NULL, expr, NULL, NULL);
            strncpy(decl->typeName, type, MAX_TOKEN_LEN-1);
            
            // Consome o final do comando
            if (curtok.type != TOK_NEWLINE && curtok.type != TOK_EOF) {
                fprintf(stderr, "Erro de sintaxe: Esperado fim de linha após declaração.\n");
                // Tenta consumir o token extra para continuar
                advance(); 
            }
            return decl;
        }
        else if (curtok.type == TOK_EQ) {
            // N_VAR_ASSIGN: ID = EXPR
            advance();
            Node *expr = parse_and_or(); 
            if (!expr) {
                fprintf(stderr, "Erro de sintaxe: Expressão esperada após '=' em atribuição.\n");
                exit(1);
            }

            // Consome o final do comando
            if (curtok.type != TOK_NEWLINE && curtok.type != TOK_EOF) {
                fprintf(stderr, "Erro de sintaxe: Esperado fim de linha após atribuição.\n");
                advance();
            }
            return make_node(N_VAR_ASSIGN, id, NULL, expr, NULL, NULL);
        }
        else if (curtok.type == TOK_LPAREN) {
            // N_EXPR_STMT (Function Call): ID ( ARGS ) \n
            Node *expr = parse_call(id);
            
            // Consome o final do comando
            if (curtok.type != TOK_NEWLINE && curtok.type != TOK_EOF) {
                fprintf(stderr, "Erro de sintaxe: Esperado fim de linha após chamada de função.\n");
                advance();
            }
            return make_node(N_EXPR_STMT, NULL, NULL, expr, NULL, NULL);
        }
        else if (curtok.type == TOK_NEWLINE || curtok.type == TOK_EOF) {
            // N_EXPR_STMT (Standalone Variable - Provavelmente erro em Sauce, mas aceito como expressão)
            
            if (is_global) {
                 fprintf(stderr, "Parse error: Standalone identifier '%s' is not a valid global command (must be a declaration or executable command).\n", id);
                 exit(1);
            }
            Node *var_expr = make_node(N_VAR, id, NULL, NULL, NULL, NULL);
            return make_node(N_EXPR_STMT, NULL, NULL, var_expr, NULL, NULL);
        }
        

        fprintf(stderr, "Unexpected token after identifier in statement: %s (Expected '[', '=' or '(' for a function call)\n", curtok.lexeme);
        exit(1);
    }

    else if (curtok.type == TOK_SAY) {
        // N_SAY: say ( EXPR )
        advance();
        expect(TOK_LPAREN); advance();
        Node *expr = parse_and_or(); 
        expect(TOK_RPAREN); advance();
        
        if (curtok.type != TOK_NEWLINE && curtok.type != TOK_EOF && curtok.type != TOK_RBRACE) {
            fprintf(stderr, "Erro de sintaxe: Esperado fim de linha após comando 'say'.\n");
            advance();
        }
        return make_node(N_SAY, NULL, NULL, expr, NULL, NULL);
    }
    
    else if (curtok.type == TOK_HEAR) {
        // N_HEAR: hear ( ID )
        advance();
        expect(TOK_LPAREN); advance();
        expect(TOK_ID);
        char varname[MAX_TOKEN_LEN]; strcpy(varname, curtok.lexeme);
        advance();
        expect(TOK_RPAREN); advance();
        
        if (curtok.type != TOK_NEWLINE && curtok.type != TOK_EOF && curtok.type != TOK_RBRACE) {
            fprintf(stderr, "Erro de sintaxe: Esperado fim de linha após comando 'hear'.\n");
            advance();
        }
        return make_node(N_HEAR, NULL, NULL, make_node(N_VAR, varname, NULL, NULL, NULL, NULL), NULL, NULL);
    }
    
    else if (curtok.type == TOK_IF) {
        // N_IF: if ( COND ) { BLOCK } [ else { BLOCK } ]
        advance();
        expect(TOK_LPAREN); advance();
        Node *cond = parse_condition();
        expect(TOK_RPAREN); advance();
        
        expect(TOK_LBRACE); advance();
        Node *then_block = parse_block_list();
        expect(TOK_RBRACE); advance();
        
        Node *else_block = NULL;
        if (curtok.type == TOK_ELSE) {
            advance();
            skip_newlines(); // Permite newline entre 'else' e '{'
            expect(TOK_LBRACE); advance();
            else_block = parse_block_list();
            expect(TOK_RBRACE); advance();
        }
        
        return make_node(N_IF, NULL, NULL, cond, else_block, then_block);
    }
    
    else if (curtok.type == TOK_RETURN) {
        // N_RETURN: return EXPR
        advance();
        Node *expr = parse_and_or(); 
        if (!expr) {
            fprintf(stderr, "Erro de sintaxe: Expressão esperada após 'return'.\n");
            exit(1);
        }
        
        if (curtok.type != TOK_NEWLINE && curtok.type != TOK_EOF && curtok.type != TOK_RBRACE) {
            fprintf(stderr, "Erro de sintaxe: Esperado fim de linha após comando 'return'.\n");
            advance();
        }
        return make_node(N_RETURN, NULL, NULL, expr, NULL, NULL);
    }
    
    else if (curtok.type == TOK_NEWLINE) {
        // Caso de múltiplas linhas vazias
        advance();
        return parse_statement(is_global); // Tenta analisar o próximo comando
    }
    
    else {
        fprintf(stderr, "Unknown start of statement: token %d ('%s')\n", curtok.type, curtok.lexeme);
        exit(1);
    }
}


static Node *parse_block_list() {
    Node *list_head = NULL;
    Node *list_current = NULL;
    
    skip_newlines();
    while (curtok.type != TOK_RBRACE && curtok.type != TOK_EOF) {
        Node *stmt = parse_statement(0);
        
        if (list_head == NULL) {
            list_head = make_node(N_STMT_LIST, NULL, NULL, stmt, NULL, NULL);
            list_current = list_head;
        } else {
            list_current->right = make_node(N_STMT_LIST, NULL, NULL, stmt, NULL, NULL);
            list_current = list_current->right;
        }
        // Nenhuma chamada a skip_newlines() aqui, pois já foi feita em parse_statement
    }
    
    return list_head;
}

// Analisa a definição de uma função
static Node *parse_function_definition() {
    expect(TOK_FN); advance();
    expect(TOK_ID);
    char fname[MAX_TOKEN_LEN]; strcpy(fname, curtok.lexeme);
    advance();

    expect(TOK_LPAREN); advance();
    
    Node *param_list = NULL;
    Node *current_param = NULL; 

    // Processa o primeiro parâmetro (se existir)
    if (curtok.type == TOK_ID) {
        char param_name[MAX_TOKEN_LEN]; strcpy(param_name, curtok.lexeme);
        advance();
        expect(TOK_LBRACK); advance();
        expect(TOK_TYPE);
        char param_type[MAX_TOKEN_LEN]; strcpy(param_type, curtok.lexeme);
        advance();
        expect(TOK_RBRACK); advance();
        
        Node *param_node = make_node(N_VAR_DECL, param_name, NULL, NULL, NULL, NULL);
        strncpy(param_node->typeName, param_type, MAX_TOKEN_LEN-1);
        
        param_list = make_node(N_STMT_LIST, NULL, NULL, param_node, NULL, NULL);
        current_param = param_list;
        
        // Processa parâmetros adicionais, separados por vírgula
        while (curtok.type == TOK_COMMA) {
            advance(); 
            
            expect(TOK_ID);
            strcpy(param_name, curtok.lexeme);
            advance();
            expect(TOK_LBRACK); advance();
            expect(TOK_TYPE);
            strcpy(param_type, curtok.lexeme);
            advance();
            expect(TOK_RBRACK); advance();

            param_node = make_node(N_VAR_DECL, param_name, NULL, NULL, NULL, NULL);
            strncpy(param_node->typeName, param_type, MAX_TOKEN_LEN-1);
            
            current_param->right = make_node(N_STMT_LIST, NULL, NULL, param_node, NULL, NULL);
            current_param = current_param->right;
        }
    }
    
    expect(TOK_RPAREN); advance();
    
    char ret_type[MAX_TOKEN_LEN] = "void";
    // Tipo de retorno explícito (opcional)
    if (curtok.type == TOK_LBRACK) {
        advance();
        expect(TOK_TYPE);
        strncpy(ret_type, curtok.lexeme, MAX_TOKEN_LEN-1);
        advance();
        expect(TOK_RBRACK); advance();
    }

    expect(TOK_LBRACE); advance();
    Node *body_list = parse_block_list();
    expect(TOK_RBRACE); advance();
    
    // Consome o newline após o fechamento da função
    skip_newlines(); 

    Node *fn_def = make_node(N_FN_DEF, fname, NULL, param_list, body_list, NULL);
    strncpy(fn_def->typeName, ret_type, MAX_TOKEN_LEN-1);

    return fn_def;
}


/* ------------------------------------------------------------
   PARSE ALL (Ponto de Entrada)
   ------------------------------------------------------------ */
void parse_all() {
    advance();
    skip_newlines();

    while (curtok.type != TOK_EOF) {
        if (curtok.type == TOK_FN) {
            Node *fn_def = parse_function_definition();
            if (fnDefCount < MAX_FN_DEFS) {
                fn_defs[fnDefCount++] = fn_def;
            } else {
                fprintf(stderr, "Erro: Limite de funções excedido.\n");
                exit(1);
            }
        } else {
            Node *stmt = parse_statement(1);
            if (globalStmtCount < MAX_FN_DEFS) { 
                global_stmts[globalStmtCount++] = stmt;
            } else {
                fprintf(stderr, "Erro: Limite de comandos globais excedido.\n");
                exit(1);
            }
        }
        skip_newlines(); 
    }
    
    generate_code("output.c", NULL); 
}