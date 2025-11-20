// codegen.c -- Gera código C a partir da Abstract Syntax Tree (AST)

#include "compiler.h"

// --- Variáveis Globais (Definidas no parser/compiler.h) ---
extern Node *fn_defs[];
extern int fnDefCount;
extern Node *global_stmts[];
extern int globalStmtCount;

static FILE *outf = NULL;

// --- Estruturas de Suporte para Semantic Analysis ---
typedef struct {
    char name[MAX_TOKEN_LEN];
    char type[MAX_TOKEN_LEN];
} Symbol;

#define MAX_SYMBOLS 512
Symbol global_symbols[MAX_SYMBOLS];
int globalSymbolCount = 0;


// --- Prototipos Internos ---
static const char *sauce_type_to_c(const char *sauce_type);
static const char *lookup_variable_type(const char *name, Node *fn_def);
static Node *find_function_def(const char *name);
static const char *get_expr_type(Node *expr, Node *fn_context);
static const char *recursive_find_return_type(Node *block_list, Node *fn_context);
static void infer_function_return_type(Node *fn_def);
static int ends_with_return(Node *block_list);

static void gen_expr(Node *n, Node *fn_context);
static void gen_statement(Node *n, Node *fn_def);
static void gen_fn_definition(Node *n);

static int is_literal_constant(Node *n) {
    if (!n) return 0;
    return n->kind == N_INT || n->kind == N_FLOAT || n->kind == N_BOOL || n->kind == N_STRING;
}

static const char* get_c_fn_name(const char *sauce_name) {
    if (strcmp(sauce_name, "main") == 0) {
        return "sauce_main";
    }
    return sauce_name;
}


// ------------------------------------------
// --- Semantic Analysis & Utilities ---
// ------------------------------------------

const char *sauce_type_to_c(const char *sauce_type) {
    if (strcmp(sauce_type, "int") == 0) return "int";
    if (strcmp(sauce_type, "float") == 0) return "double";
    if (strcmp(sauce_type, "string") == 0 || strcmp(sauce_type, "text") == 0) return "char*";
    if (strcmp(sauce_type, "bool") == 0 || strcmp(sauce_type, "boolean") == 0) return "int";
    return "void";
}

Node *find_function_def(const char *name) {
    for (int i = 0; i < fnDefCount; i++) {
        if (strcmp(fn_defs[i]->name, name) == 0) {
            return fn_defs[i];
        }
    }
    return NULL;
}

const char *lookup_variable_type(const char *name, Node *fn_def) {
    // 1. Verificar escopo da função (parâmetros e variáveis locais)
    if (fn_def != NULL) {
        Node *param_wrapper = fn_def->left;
        while (param_wrapper) {
            Node *param = param_wrapper->left;
            if (param && strcmp(param->name, name) == 0) {
                return param->typeName;
            }
            param_wrapper = param_wrapper->right;
        }
    }

    // 2. Verificar escopo global (símbolos já registrados)
    for (int i = 0; i < globalSymbolCount; i++) {
        if (strcmp(global_symbols[i].name, name) == 0) {
            return global_symbols[i].type;
        }
    }
    
    // 3. Verifica declarações globais na AST (se ainda não foram registradas)
    for (int i = 0; i < globalStmtCount; i++) {
        Node *stmt = global_stmts[i];
        if (stmt->kind == N_VAR_DECL && strcmp(stmt->name, name) == 0) {
            if (globalSymbolCount < MAX_SYMBOLS) {
                strcpy(global_symbols[globalSymbolCount].name, stmt->name);
                strcpy(global_symbols[globalSymbolCount].type, stmt->typeName);
                globalSymbolCount++;
            }
            return stmt->typeName;
        }
    }

    return NULL;
}

const char *get_expr_type(Node *expr, Node *fn_context) {
    if (!expr) return "void";

    switch (expr->kind) {
        case N_INT: return "int";
        case N_FLOAT: return "float";
        case N_BOOL: return "boolean";
        case N_STRING: return "text"; 
        
        case N_VAR: {
            const char *type = lookup_variable_type(expr->name, fn_context);
            if (!type) {
                fprintf(stderr, "Erro Semântico: Variável '%s' não declarada.\n", expr->name);
                exit(1);
            }
            return type;
        }
        case N_FN_CALL: {
            Node *fn_def = find_function_def(expr->name);
            if (!fn_def) {
                fprintf(stderr, "Erro Semântico: Função '%s' não definida.\n", expr->name);
                exit(1);
            }
            return fn_def->typeName;
        }

        case N_ADD: case N_SUB: case N_MUL: case N_DIV: {
            const char *left_type = get_expr_type(expr->left, fn_context);
            const char *right_type = get_expr_type(expr->right, fn_context);
            if (strcmp(left_type, "float") == 0 || strcmp(right_type, "float") == 0) return "float";
            if (strcmp(left_type, "int") == 0 && strcmp(right_type, "int") == 0) return "int";
            
            fprintf(stderr, "Erro Semântico: Tipos incompatíveis para operação aritmética: %s e %s\n", left_type, right_type);
            exit(1);
        }
        case N_GT: case N_LT: case N_EQ_CMP: case N_NEQ: 
        case N_GTE: case N_LTE: 
        case N_AND: case N_OR: case N_NOT: 
            return "boolean";
            
        default:
            return "void";
    }
}

const char *recursive_find_return_type(Node *block_list, Node *fn_context) {
    Node *stmt_wrapper = block_list;
    while (stmt_wrapper) {
        Node *current_stmt_node = stmt_wrapper->left;
        if (current_stmt_node) {
            if (current_stmt_node->kind == N_RETURN) {
                if (current_stmt_node->explicitReturnType[0] != '\0') {
                    return current_stmt_node->explicitReturnType;
                }
                return get_expr_type(current_stmt_node->left, fn_context);
                
            } else if (current_stmt_node->kind == N_IF) {
                const char *type_in_then = recursive_find_return_type(current_stmt_node->right, fn_context);
                const char *type_in_else = current_stmt_node->mid ? recursive_find_return_type(current_stmt_node->mid, fn_context) : "void";
                
                if (strcmp(type_in_then, "void") != 0 && strcmp(type_in_then, type_in_else) == 0) {
                    return type_in_then;
                }
            }
        }
        stmt_wrapper = stmt_wrapper->right;
    }
    return "void";
}

void infer_function_return_type(Node *fn_def) {
    if (fn_def->typeName[0] == '\0' || strcmp(fn_def->typeName, "void") == 0) {
        const char *inferred_type = recursive_find_return_type(fn_def->mid, fn_def);
        if (inferred_type != NULL) {
            strncpy(fn_def->typeName, inferred_type, MAX_TOKEN_LEN-1);
            fn_def->typeName[MAX_TOKEN_LEN-1] = '\0';
        }
    }
    if (strcmp(fn_def->typeName, "text") == 0) {
        strcpy(fn_def->typeName, "string"); 
    }
}

// CORRIGIDO: Checa se o último nó é N_RETURN
static int ends_with_return(Node *block_list) {
    if (!block_list) return 0;
    
    Node *current = block_list;
    Node *last_stmt = NULL;

    while (current) {
        last_stmt = current->left;
        if (current->right == NULL) {
            break; 
        }
        current = current->right;
    }

    if (last_stmt && last_stmt->kind == N_RETURN) {
        return 1;
    }
    return 0;
}

// ------------------------------------------
// --- Code Generation Core ---
// ------------------------------------------

static void gen_expr(Node *n, Node *fn_context) {
    if (!n) return;

    switch (n->kind) {
        case N_INT:
        case N_FLOAT:
        case N_STRING:
            fprintf(outf, "%s", n->text);
            break;
            
        case N_BOOL:
            fprintf(outf, "%s", strcmp(n->text, "true") == 0 ? "1" : "0");
            break;

        case N_VAR:
            fprintf(outf, "%s", n->name);
            break;

        case N_FN_CALL:
            fprintf(outf, "%s(", get_c_fn_name(n->name));
            Node *arg_wrapper = n->left;
            while (arg_wrapper) {
                gen_expr(arg_wrapper->left, fn_context); 
                arg_wrapper = arg_wrapper->right;
                if (arg_wrapper) {
                    fprintf(outf, ", ");
                }
            }
            fprintf(outf, ")");
            break;
        
        case N_AND:
        case N_OR:
        case N_NOT:
        case N_ADD: case N_SUB: case N_MUL: case N_DIV: 
        case N_GT: case N_EQ_CMP: case N_LT: case N_NEQ: 
        case N_GTE: case N_LTE: 
        
            if (n->kind == N_NOT) {
                fprintf(outf, "(!");
                gen_expr(n->left, fn_context);
                fprintf(outf, ")");
                break;
            }

            fprintf(outf, "("); 
            gen_expr(n->left, fn_context); 
            
            if (n->kind == N_AND) fprintf(outf, " && "); 
            else if (n->kind == N_OR) fprintf(outf, " || "); 
            else if (n->kind == N_ADD) fprintf(outf, " + ");
            else if (n->kind == N_SUB) fprintf(outf, " - ");
            else if (n->kind == N_MUL) fprintf(outf, " * ");
            else if (n->kind == N_DIV) fprintf(outf, " / ");
            else if (n->kind == N_GT) fprintf(outf, " > ");
            else if (n->kind == N_LT) fprintf(outf, " < ");
            else if (n->kind == N_EQ_CMP) fprintf(outf, " == ");
            else if (n->kind == N_NEQ) fprintf(outf, " != ");
            else if (n->kind == N_GTE) fprintf(outf, " >= ");
            else if (n->kind == N_LTE) fprintf(outf, " <= ");
            
            gen_expr(n->right, fn_context); 
            fprintf(outf, ")"); 
            break;
            
        default:
            fprintf(stderr, "Erro Interno: Expressão de nó desconhecida: %d\n", n->kind);
            exit(1);
    }
}

static void gen_statement(Node *n, Node *fn_def) {
    if (!n) return;

    switch (n->kind) {
        case N_VAR_DECL: {
            // Este caso só deve ocorrer para declarações LOCAIS.
            const char *c_type = sauce_type_to_c(n->typeName);
            
            fprintf(outf, "    %s %s", c_type, n->name);
            
            if (n->left) {
                fprintf(outf, " = ");
                gen_expr(n->left, fn_def);
            } else if (strcmp(c_type, "char*") == 0) {
                 fprintf(outf, " = NULL"); 
            } else {
                 fprintf(outf, " = 0");
            }
            
            fprintf(outf, ";\n");
            break;
        }
        
        case N_VAR_ASSIGN: {
            const char *sauce_type = lookup_variable_type(n->name, fn_def);

            if (strcmp(sauce_type, "text") == 0 || strcmp(sauce_type, "string") == 0) {
                fprintf(outf, "    if (%s != NULL) free(%s);\n", n->name, n->name);
                fprintf(outf, "    %s = strdup(", n->name);
                gen_expr(n->left, fn_def);
                fprintf(outf, ");\n");
            } else {
                fprintf(outf, "    %s = ", n->name);
                gen_expr(n->left, fn_def);
                fprintf(outf, ";\n");
            }
            break;
        }

        case N_SAY: {
            Node *expr = n->left;
            const char *type = get_expr_type(expr, fn_def);
            
            fprintf(outf, "    printf(");
            
            if (strcmp(type, "int") == 0) {
                fprintf(outf, "\"%%d\\n\", ");
            } else if (strcmp(type, "float") == 0) {
                fprintf(outf, "\"%%f\\n\", ");
            } else if (strcmp(type, "text") == 0 || strcmp(type, "string") == 0) {
                fprintf(outf, "\"%%s\\n\", ");
            } else if (strcmp(type, "boolean") == 0) {
                fprintf(outf, "\"%%s\\n\", ("); 
                gen_expr(expr, fn_def); 
                fprintf(outf, ") ? \"true\" : \"false\""); 
                fprintf(outf, ");\n"); 
                break;
            }
            else {
                 fprintf(outf, "\"Erro: Tipo desconhecido\\n\");\n");
                 break;
            }
            
            gen_expr(expr, fn_def);
            fprintf(outf, ");\n");
            break;
        }
        
        case N_HEAR: {
            const char *var_name = n->left->name;
            const char *sauce_type = lookup_variable_type(var_name, fn_def);
            const char *c_type = sauce_type_to_c(sauce_type);
            
            fprintf(outf, "    printf(\"\\n> \");\n");
            
            if (strcmp(c_type, "int") == 0) {
                fprintf(outf, "    if (scanf(\"%%d\", &%s) != 1) { /* erro */ } \n", var_name);
            } else if (strcmp(c_type, "double") == 0) {
                fprintf(outf, "    if (scanf(\"%%lf\", &%s) != 1) { /* erro */ } \n", var_name);
            } else if (strcmp(c_type, "char*") == 0) {
                 fprintf(outf, "    { char _buf[1024]; if (!fgets(_buf, sizeof(_buf), stdin)) _buf[0]='\\0'; _buf[strcspn(_buf, \"\\n\")]='\\0'; if (%s != NULL) free(%s); %s = strdup(_buf); }\n", var_name, var_name, var_name);
            } else {
                fprintf(outf, "    // Tipo '%s' nao suporta HEAR.\n", sauce_type);
            }
            
            if (strcmp(c_type, "int") == 0 || strcmp(c_type, "double") == 0) {
                 fprintf(outf, "    { int _c=getchar(); while(_c!='\\n' && _c!=EOF) _c=getchar(); }\n");
            }
            break;
        }
        
        case N_IF: {
            fprintf(outf, "    if (");
            gen_expr(n->left, fn_def); 
            fprintf(outf, ") {\n");
            
            Node *body_stmt = n->right;
            while (body_stmt) {
                gen_statement(body_stmt->left, fn_def);
                body_stmt = body_stmt->right;
            }
            
            fprintf(outf, "    }");
            
            if (n->mid) {
                fprintf(outf, " else {\n");
                Node *else_stmt = n->mid;
                while (else_stmt) {
                    gen_statement(else_stmt->left, fn_def);
                    else_stmt = else_stmt->right;
                }
                fprintf(outf, "    }");
            }
            fprintf(outf, "\n");
            break;
        }

        case N_RETURN:
            fprintf(outf, "    return ");
            
            if (n->explicitReturnType[0] != '\0') {
                const char *c_type = sauce_type_to_c(n->explicitReturnType);
                fprintf(outf, "(%s)", c_type);
            }
            
            gen_expr(n->left, fn_def);
            fprintf(outf, ";\n");
            break;

        case N_EXPR_STMT:
            fprintf(outf, "    ");
            gen_expr(n->left, fn_def);
            fprintf(outf, ";\n");
            break;
            
        default:
            fprintf(stderr, "Erro Interno: Comando de nó desconhecido para geração: %d\n", n->kind);
            exit(1);
    }
}

static void gen_fn_definition(Node *n) {
    const char *return_type = sauce_type_to_c(n->typeName);
    const char *fn_name_c = get_c_fn_name(n->name); 

    fprintf(outf, "\n%s %s(", return_type, fn_name_c);

    Node *param_wrapper = n->left;
    while (param_wrapper) {
        Node *param = param_wrapper->left;
        fprintf(outf, "%s %s", sauce_type_to_c(param->typeName), param->name);
        param_wrapper = param_wrapper->right;
        if (param_wrapper) {
            fprintf(outf, ", ");
        }
    }
    fprintf(outf, ") {\n");

    Node *stmt_wrapper = n->mid;
    while (stmt_wrapper) {
        gen_statement(stmt_wrapper->left, n);
        stmt_wrapper = stmt_wrapper->right;
    }
    
    // Retorno de segurança CORRIGIDO
    if (strcmp(return_type, "void") != 0 && !ends_with_return(n->mid)) {
        fprintf(outf, "\n    // Retorno de segurança (para garantir um caminho de saída)\n");
        if (strcmp(return_type, "int") == 0 || strcmp(return_type, "boolean") == 0) {
            fprintf(outf, "    return 0;\n");
        } else if (strcmp(return_type, "double") == 0) {
            fprintf(outf, "    return 0.0;\n");
        } else if (strcmp(return_type, "char*") == 0) {
            fprintf(outf, "    return NULL;\n");
        }
    }

    fprintf(outf, "}\n");
}


// ------------------------------------------
// --- Ponto de Entrada Global da Geração de Código (CORRIGIDO) ---
// ------------------------------------------

void generate_code(const char *out_c, Node *program_root) {
    (void)program_root; 

    outf = fopen(out_c, "w");
    if (!outf) { perror("Erro ao abrir arquivo de saída"); exit(1); }

    fprintf(outf, "/* Código C gerado pelo compilador Sauce (AST-based) */\n");
    
    // Includes
    fprintf(outf, "#ifndef _POSIX_C_SOURCE\n");
    fprintf(outf, "#define _POSIX_C_SOURCE 200809L\n");
    fprintf(outf, "#endif\n");
    
    fprintf(outf, "#include <stdio.h>\n");
    fprintf(outf, "#include <stdlib.h>\n");
    fprintf(outf, "#include <string.h>\n");
    fprintf(outf, "\n");
    
    // 1. INFERÊNCIA DE TIPO DE RETORNO
    for (int i = 0; i < fnDefCount; i++) {
        infer_function_return_type(fn_defs[i]);
    }
    
    // 2. Protótipos de Funções
    for (int i = 0; i < fnDefCount; i++) {
        Node *fn = fn_defs[i];
        fprintf(outf, "%s %s(", sauce_type_to_c(fn->typeName), get_c_fn_name(fn->name));

        Node *param_wrapper = fn->left;
        while (param_wrapper) {
            Node *param = param_wrapper->left;
            fprintf(outf, "%s", sauce_type_to_c(param->typeName));
            param_wrapper = param_wrapper->right;
            if (param_wrapper) {
                fprintf(outf, ", ");
            }
        }
        fprintf(outf, ");\n");
    }
    fprintf(outf, "\n");
    
    // 3. Variáveis Globais (Declaração C no escopo global)
    globalSymbolCount = 0; 
    
    for (int i = 0; i < globalStmtCount; i++) {
        Node *stmt = global_stmts[i];
        if (stmt->kind == N_VAR_DECL) {
            const char *sauce_type = stmt->typeName;
            const char *c_type = sauce_type_to_c(sauce_type);
            
            // Registra no símbolo global
            if (globalSymbolCount < MAX_SYMBOLS) {
                strcpy(global_symbols[globalSymbolCount].name, stmt->name);
                strcpy(global_symbols[globalSymbolCount].type, sauce_type);
                globalSymbolCount++;
            }
            
            // Apenas declara e inicializa em 0/NULL (a inicialização correta ocorrerá no main para garantir a ordem)
            if (strcmp(c_type, "char*") == 0) {
                fprintf(outf, "%s %s = NULL;\n", c_type, stmt->name);
            } else {
                fprintf(outf, "%s %s = 0;\n", c_type, stmt->name); 
            }
        }
    }
    fprintf(outf, "\n");
    
    // 4. Geração de Definições de Funções (corpo)
    for (int i = 0; i < fnDefCount; i++) {
        gen_fn_definition(fn_defs[i]);
    }
    
    // 5. Bloco principal (main)
    fprintf(outf, "\nint main(void) {\n");
    
    // CORREÇÃO CRÍTICA: Percorre todos os comandos globais na ORDEM ORIGINAL
    for (int i = 0; i < globalStmtCount; i++) {
        Node *stmt = global_stmts[i];
        
        if (stmt->kind == N_VAR_DECL) {
            // Se for N_VAR_DECL, geramos a ATRIBUIÇÃO/INICIALIZAÇÃO para respeitar a ordem.
            const char *var_name = stmt->name;
            const char *sauce_type = lookup_variable_type(var_name, NULL); 
            
            if (strcmp(sauce_type, "text") == 0 || strcmp(sauce_type, "string") == 0) {
                // Atribuição de string (free + strdup)
                fprintf(outf, "    if (%s != NULL) free(%s);\n", var_name, var_name);
                fprintf(outf, "    %s = strdup(", var_name);
                gen_expr(stmt->left, NULL); // Sem contexto de função
                fprintf(outf, ");\n");
            } else {
                // Atribuição simples
                fprintf(outf, "    %s = ", var_name);
                gen_expr(stmt->left, NULL); // Sem contexto de função
                fprintf(outf, ";\n");
            }
        } 
        else {
            // Comandos executáveis (N_SAY, N_EXPR_STMT, N_IF, etc.)
            gen_statement(stmt, NULL);
        }
    }
    
    // Cleanup (free) para strings globais alocadas
    int needs_cleanup = 0;
    for (int i = 0; i < globalStmtCount; i++) {
        Node *stmt = global_stmts[i];
        if (stmt->kind == N_VAR_DECL && (strcmp(stmt->typeName, "text") == 0 || strcmp(stmt->typeName, "string") == 0)) {
            needs_cleanup = 1;
            break;
        }
    }
    
    if (needs_cleanup) {
        fprintf(outf, "\n    // Cleanup para variaveis text globais\n");
        for (int i = 0; i < globalStmtCount; i++) {
            Node *stmt = global_stmts[i];
            if (stmt->kind == N_VAR_DECL && (strcmp(stmt->typeName, "text") == 0 || strcmp(stmt->typeName, "string") == 0)) {
                fprintf(outf, "    if (%s != NULL) free(%s);\n", stmt->name, stmt->name);
            }
        }
    }
    
    // Fim do main
    fprintf(outf, "    return 0;\n");
    fprintf(outf, "}\n");

    fclose(outf);
}