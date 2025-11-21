#include "compiler.h"

static const char *SRC = NULL;
static int POS = 0;

static int peek() {
    return SRC[POS];
}

static int nextchar() {
    int c = SRC[POS];
    if (c != '\0')
        POS++;
    return c;
}

static void skip_spaces() {
    // Adiciona tratamento para caracteres de controle e outros espaços em branco comuns
    while (1) {
        int c = peek();
        // Inclui espaços normais, tab, retorno de carro e form feed
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f') 
            nextchar();
        else
            break;
    }
}

void lexer_init_from_string(const char *s) {
    SRC = s;
    POS = 0;
}

Token next_token() {
    Token tok;
    tok.lexeme[0] = '\0';
    tok.type = TOK_EOF;

    skip_spaces();

    int c = peek();

    // EOF
    if (c == '\0') {
        strcpy(tok.lexeme, "EOF");
        tok.type = TOK_EOF;
        return tok;
    }

    // NEWLINE
    if (c == '\n') {
        nextchar();
        tok.type = TOK_NEWLINE;
        strcpy(tok.lexeme, "\\n");
        return tok;
    }

    // identifier or keyword or type
    if (isalpha(c) || c == '_') {
        int start = POS;
        
        // O loop consome o primeiro caractere que já foi verificado e continua.
        while (isalnum(peek()) || peek() == '_')
            nextchar();
            
        // POS agora está apontando para o primeiro caractere após o token.
        int len = POS - start;
        strncpy(tok.lexeme, SRC + start, len);
        tok.lexeme[len] = '\0';

        // --- PALAVRAS-CHAVE E LITERAIS ---
        if (strcmp(tok.lexeme, "say") == 0) { tok.type = TOK_SAY; return tok; }
        else if (strcmp(tok.lexeme, "hear") == 0) { tok.type = TOK_HEAR; return tok; }
        else if (strcmp(tok.lexeme, "if") == 0) { tok.type = TOK_IF; return tok; }
        else if (strcmp(tok.lexeme, "else") == 0) { tok.type = TOK_ELSE; return tok; }
        else if (strcmp(tok.lexeme, "fn") == 0) { tok.type = TOK_FN; return tok; }
        else if (strcmp(tok.lexeme, "return") == 0) { tok.type = TOK_RETURN; return tok; }
        else if (strcmp(tok.lexeme, "true") == 0) { tok.type = TOK_TRUE; return tok; }
        else if (strcmp(tok.lexeme, "false") == 0) { tok.type = TOK_FALSE; return tok; }
        else if (strcmp(tok.lexeme, "and") == 0) { tok.type = TOK_AND; return tok; }
        else if (strcmp(tok.lexeme, "or") == 0) { tok.type = TOK_OR; return tok; }
        else if (strcmp(tok.lexeme, "not") == 0) { tok.type = TOK_NOT; return tok; }
        // types
        else if (!strcmp(tok.lexeme, "int") ||
            !strcmp(tok.lexeme, "float") ||
            !strcmp(tok.lexeme, "text") ||
            !strcmp(tok.lexeme, "boolean"))
        {
            tok.type = TOK_TYPE;
            return tok;
        }

        // Se não for palavra-chave nem tipo, é um identificador
        tok.type = TOK_ID;
        return tok;
    }

    // number
    if (isdigit(c)) {
        int start = POS;
        
        // O loop consome o primeiro dígito e continua
        while (isdigit(peek())) nextchar();

        if (peek() == '.') {
            nextchar();
            while (isdigit(peek())) nextchar();
        }
        
        // POS agora está apontando para o primeiro caractere após o token.
        int len = POS - start;
        strncpy(tok.lexeme, SRC + start, len);
        tok.lexeme[len] = '\0';

        tok.type = TOK_NUMBER;
        return tok;
    }

    // string
    if (c == '"') {
        int start = POS; 

        nextchar(); // Consome aspa de abertura
        while (peek() != '"' && peek() != '\0' && peek() != '\n') 
            nextchar();

        if (peek() == '"')
            nextchar(); // Consome aspa de fechamento
        else {
            fprintf(stderr, "String não fechada ou quebra de linha inesperada na string!\n");
            exit(1);
        }

        // Pega apenas o conteúdo interno (exclui as aspas)
        int content_start = start + 1;
        int content_end = POS - 1; 
        int len = content_end - content_start; 
        
        if (len < 0) {
            len = 0; 
        }
        
        strncpy(tok.lexeme, SRC + content_start, len);
        tok.lexeme[len] = '\0';

        tok.type = TOK_STRING;
        return tok;
    }

    // symbols and operators
    switch (c) {
        case '(':
            nextchar(); tok.type = TOK_LPAREN; strcpy(tok.lexeme, "("); return tok;
        case ')':
            nextchar(); tok.type = TOK_RPAREN; strcpy(tok.lexeme, ")"); return tok;
        case '{':
            nextchar(); tok.type = TOK_LBRACE; strcpy(tok.lexeme, "{"); return tok;
        case '}':
            nextchar(); tok.type = TOK_RBRACE; strcpy(tok.lexeme, "}"); return tok;
        case '[':
            nextchar(); tok.type = TOK_LBRACK; strcpy(tok.lexeme, "["); return tok;
        case ']':
            nextchar(); tok.type = TOK_RBRACK; strcpy(tok.lexeme, "]"); return tok;
        case ',':
            nextchar(); tok.type = TOK_COMMA; strcpy(tok.lexeme, ","); return tok;
        case ';':
            nextchar(); tok.type = TOK_SEMI; strcpy(tok.lexeme, ";"); return tok;

        // Operadores de 1 ou 2 caracteres
        case '=':
            nextchar();
            if (peek() == '=') { // ==
                nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "=="); return tok;
            }
            tok.type = TOK_EQ; // = (Atribuição)
            strcpy(tok.lexeme, "=");
            return tok;
        case '!':
            nextchar();
            if (peek() == '=') { // !=
                nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "!="); return tok;
            }
            fprintf(stderr, "Caractere inválido no lexer: '!' (apenas '!=' é suportado)\n");
            exit(1);
        case '&':
             fprintf(stderr, "Operador '&' inválido. Use a palavra-chave 'and'.\n");
             exit(1);
        case '|':
             fprintf(stderr, "Operador '|' inválido. Use a palavra-chave 'or'.\n");
             exit(1);

        // Operadores de 1 ou 2 caracteres (RELACIONAIS)
        case '>':
            nextchar();
            if (peek() == '=') {
                nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, ">="); return tok;
            }
            tok.type = TOK_OPERATOR; strcpy(tok.lexeme, ">"); return tok;
        case '<':
            nextchar();
            if (peek() == '=') {
                nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "<="); return tok;
            }
            tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "<"); return tok;

        // Operadores de 1 caractere (ARITMÉTICOS)
        case '+':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "+"); return tok;
        case '-':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "-"); return tok;
        case '*':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "*"); return tok;
        case '/':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "/"); return tok;
    }

    // Se chegou aqui, é um caractere que não reconhecemos.
    fprintf(stderr, "Caractere inválido no lexer: '%c' (código: %d)\n", c, c);
    exit(1);
}