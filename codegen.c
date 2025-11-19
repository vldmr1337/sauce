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


// --- Prototipos Internos (Semantic Analysis e Geração) ---
static const char *sauce_type_to_c(const char *sauce_type);
static const char *lookup_variable_type(const char *name, Node *fn_def);
static Node *find_function_def(const char *name);
static const char *get_expr_type(Node *expr, Node *fn_context);
static const char *recursive_find_return_type(Node *block_list, Node *fn_context);
static void infer_function_return_type(Node *fn_def);

static void gen_expr(Node *n, Node *fn_context);
static void gen_statement(Node *n, Node *fn_def);
static void gen_fn_definition(Node *n);

// --- Semantic Analysis & Utilities (Implementação Completa) ---

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
    // 1. Verificar escopo da função (parâmetros)
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
            // Registra e retorna
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
        case N_GT: case N_LT: case N_EQ_CMP: case N_NEQ: case N_AND: case N_OR:
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
        strncpy(fn_def->typeName, inferred_type, MAX_TOKEN_LEN-1);
        fn_def->typeName[MAX_TOKEN_LEN-1] = '\0';
    }
    if (strcmp(fn_def->typeName, "text") == 0) {
        strcpy(fn_def->typeName, "string");
    }
}


// --- Code Generation Core (Utiliza outf diretamente) ---

static void gen_expr(Node *n, Node *fn_context) {
    if (!n) return;

    switch (n->kind) {
        case N_INT:
        case N_FLOAT:
            fprintf(outf, "%s", n->text);
            break;
            
        case N_BOOL:
            fprintf(outf, "%s", strcmp(n->text, "true") == 0 ? "1" : "0");
            break;

        case N_STRING:
            // FIX: O n->text (lexeme) já contém as aspas. Apenas imprima o conteúdo.
            fprintf(outf, "%s", n->text);
            break;

        case N_VAR:
            fprintf(outf, "%s", n->name);
            break;

        case N_FN_CALL:
            fprintf(outf, "%s(", n->name);
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

        case N_ADD: case N_SUB: case N_MUL: case N_DIV: case N_GT: case N_EQ_CMP:
            fprintf(outf, "("); 
            gen_expr(n->left, fn_context); 
            
            if (n->kind == N_ADD) fprintf(outf, " + ");
            else if (n->kind == N_SUB) fprintf(outf, " - ");
            else if (n->kind == N_MUL) fprintf(outf, " * ");
            else if (n->kind == N_DIV) fprintf(outf, " / ");
            else if (n->kind == N_GT) fprintf(outf, " > ");
            else if (n->kind == N_EQ_CMP) fprintf(outf, " == ");
            
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
            const char *sauce_type = n->typeName;
            const char *c_type = sauce_type_to_c(sauce_type);
            
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
                // Para atribuição de strings, use free e strdup
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
            if (strcmp(type, "int") == 0 || strcmp(type, "boolean") == 0) {
                fprintf(outf, "\"%%d\\n\", ");
            } else if (strcmp(type, "float") == 0) {
                fprintf(outf, "\"%%f\\n\", ");
            } else if (strcmp(type, "text") == 0 || strcmp(type, "string") == 0) {
                fprintf(outf, "\"%%s\\n\", ");
            } else {
                 fprintf(outf, "\"UNKNOWN_TYPE\\n\"");
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
                fprintf(outf, "    if (scanf(\"%%d\", &%s) != 1) { /* error handling */ } \n", var_name);
            } else if (strcmp(c_type, "double") == 0) {
                fprintf(outf, "    if (scanf(\"%%lf\", &%s) != 1) { /* error handling */ } \n", var_name);
            } else if (strcmp(c_type, "char*") == 0) {
                 fprintf(outf, "    { char _buf[1024]; if (!fgets(_buf, sizeof(_buf), stdin)) _buf[0]='\\0'; _buf[strcspn(_buf, \"\\n\")]='\\0'; if (%s != NULL) free(%s); %s = strdup(_buf); }\n", var_name, var_name, var_name);
            } else {
                fprintf(outf, "    // Tipo '%s' nao suporta HEAR.\n", sauce_type);
            }
            
            // Limpa o buffer do teclado após scanf (necessário para int/double)
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
    fprintf(outf, "\n%s %s(", return_type, n->name);

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
    
    // Retorno de segurança
    if (strcmp(return_type, "void") != 0 && (n->mid == NULL || n->mid->left->kind != N_RETURN)) {
        fprintf(outf, "    // Retorno de segurança\n");
        if (strcmp(return_type, "int") == 0) {
            fprintf(outf, "    return 0;\n");
        } else if (strcmp(return_type, "double") == 0) {
            fprintf(outf, "    return 0.0;\n");
        } else if (strcmp(return_type, "char*") == 0) {
            fprintf(outf, "    return NULL;\n");
        }
    }


    fprintf(outf, "}\n");
}


// --- Ponto de Entrada Global da Geração de Código ---

void generate_code(const char *out_c, Node *program_root) {
    // Para remover o aviso de parâmetro não utilizado
    (void)program_root; 

    outf = fopen(out_c, "w");
    if (!outf) { perror("Erro ao abrir arquivo de saída"); exit(1); }

    fprintf(outf, "/* Código C gerado pelo compilador Sauce (AST-based) */\n");
    
    // FIX CRÍTICO: Garante que strdup e fmemopen sejam declarados corretamente
    fprintf(outf, "#ifndef _POSIX_C_SOURCE\n");
    fprintf(outf, "#define _POSIX_C_SOURCE 200809L\n");
    fprintf(outf, "#endif\n");
    
    fprintf(outf, "#include <stdio.h>\n");
    fprintf(outf, "#include <stdlib.h>\n");
    fprintf(outf, "#include <string.h>\n");
    fprintf(outf, "\n");
    
    // 1. INFERÊNCIA DE TIPO DE RETORNO (passo semântico)
    for (int i = 0; i < fnDefCount; i++) {
        infer_function_return_type(fn_defs[i]);
    }
    
    // 2. Protótipos de Funções
    for (int i = 0; i < fnDefCount; i++) {
        Node *fn = fn_defs[i];
        fprintf(outf, "%s %s(", sauce_type_to_c(fn->typeName), fn->name);

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
    
    // --- Variáveis Globais (Declaração no Escopo Global C) ---
    char global_text_init_code[MAX_FN_DEFS][512];
    int global_text_count = 0;
    
    // Percorre comandos globais para gerar declarações e registrar símbolos
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
            
            if (strcmp(c_type, "char*") == 0) {
                // Declara como NULL globalmente
                fprintf(outf, "%s %s = NULL;\n", c_type, stmt->name);
                
                // --- Bloco de Inicialização de String Corrigido (Usando fmemopen seguro) ---
                char expr_buf[256] = {0}; 
                
                FILE *temp_out = fmemopen(expr_buf, sizeof(expr_buf), "w");
                
                if (!temp_out) {
                    fprintf(stderr, "Erro FATAL: fmemopen falhou. Seu ambiente não suporta esta função corretamente. \n");
                    exit(1);
                }

                FILE *old_outf = outf;
                outf = temp_out;
                gen_expr(stmt->left, NULL); // Escreve a expressão no buffer
                
                fflush(outf); 
                
                outf = old_outf; 
                
                long current_pos = ftell(temp_out);
                if ((long unsigned int)current_pos < sizeof(expr_buf)) {
                    expr_buf[current_pos] = '\0'; 
                } else {
                    expr_buf[sizeof(expr_buf) - 1] = '\0';
                }

                fclose(temp_out); 
                
                snprintf(global_text_init_code[global_text_count], 512, "    %s = strdup(%s);\n", stmt->name, expr_buf);
                global_text_count++;
                
            } else {
                // Inicialização normal de primitivos
                fprintf(outf, "%s %s = ", c_type, stmt->name);
                gen_expr(stmt->left, NULL);
                fprintf(outf, ";\n");
            }
        }
    }
    fprintf(outf, "\n");
    
    // 3. Geração de Definições de Funções (corpo)
    for (int i = 0; i < fnDefCount; i++) {
        gen_fn_definition(fn_defs[i]);
    }
    
    // 4. Bloco principal (main) - ÚNICA DEFINIÇÃO
    fprintf(outf, "\nint main(void) {\n");
    
    // Inicialização de strings globais (tempo de execução)
    for(int i = 0; i < global_text_count; i++) {
        fprintf(outf, "%s", global_text_init_code[i]);
    }

    // Geração de Comandos Globais Executáveis
    for (int i = 0; i < globalStmtCount; i++) {
        Node *stmt = global_stmts[i];
        if (stmt->kind != N_VAR_DECL) {
            gen_statement(stmt, NULL);
        }
    }
    
    // Cleanup (free) para strings globais alocadas
    if (global_text_count > 0) {
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