// compiler.c -- Orquestra o Lexer, Parser e Codegen (Fluxo AST)

#include "compiler.h"

#define MAX_SRC 65536

extern void lexer_init_from_string(const char*);
// extern Token next_token(); // Não é necessário aqui, usado por advance()
// extern void cg_finalize(); // Não é mais necessário no fluxo AST
// extern void parse_all(); // Protótipo já está em compiler.h

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s file.sauce\n", argv[0]);
        return 1;
    }
    const char *infile = argv[1];
    
    // 1. Leitura do arquivo fonte
    FILE *f = fopen(infile, "r");
    if (!f) { perror("fopen"); return 1; }
    char *buf = malloc(MAX_SRC);
    size_t r = fread(buf,1,MAX_SRC-1,f);
    buf[r] = '\0';
    fclose(f);
    
    // 2. Inicializa Lexer
    lexer_init_from_string(buf);
    
    // 3. Parser (Constrói a AST e chama generate_code internamente)
    // FIX: parse_all não recebe mais parâmetros
    parse_all(); 

    free(buf);
    
    // 4. Compila output.c -> app (mantido como estava no seu original)
    fprintf(stderr, "Compiling output.c -> app\n");
    int rc = system("cc -std=c11 -Wall -Wextra -O2 output.c -o app");
    if (rc != 0) {
        fprintf(stderr, "Compilation of output.c failed with error code %d\n", rc);
        return 1;
    }
    
    fprintf(stderr, "Success! Executable 'app' created.\n");
    return 0;
}