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
    while (1) {
        int c = peek();
        if (c == ' ' || c == '\t' || c == '\r') 
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
        while (isalnum(peek()) || peek() == '_')
            nextchar();

        int len = POS - start;
        strncpy(tok.lexeme, SRC + start, len);
        tok.lexeme[len] = '\0';

        // keywords
        if (strcmp(tok.lexeme, "say") == 0) { tok.type = TOK_SAY; return tok; }
        if (strcmp(tok.lexeme, "hear") == 0) { tok.type = TOK_HEAR; return tok; }
        if (strcmp(tok.lexeme, "if") == 0) { tok.type = TOK_IF; return tok; }
        if (strcmp(tok.lexeme, "else") == 0) { tok.type = TOK_ELSE; return tok; }
        if (strcmp(tok.lexeme, "fn") == 0) { tok.type = TOK_FN; return tok; }
        if (strcmp(tok.lexeme, "return") == 0) { tok.type = TOK_RETURN; return tok; }

        // types
        if (!strcmp(tok.lexeme, "int") ||
            !strcmp(tok.lexeme, "float") ||
            !strcmp(tok.lexeme, "text") ||
            !strcmp(tok.lexeme, "boolean"))
        {
            tok.type = TOK_TYPE;
            return tok;
        }

        tok.type = TOK_ID;
        return tok;
    }

    // number
    if (isdigit(c)) {
        int start = POS;

        while (isdigit(peek())) nextchar();

        if (peek() == '.') {
            nextchar();
            while (isdigit(peek())) nextchar();
        }

        int len = POS - start;
        strncpy(tok.lexeme, SRC + start, len);
        tok.lexeme[len] = '\0';

        tok.type = TOK_NUMBER;
        return tok;
    }

    // string
    if (c == '"') {
        int start = POS; // CAPTURA DA POSIÇÃO INICIAL (INCLUINDO A CITAÇÃO DE ABERTURA)

        nextchar(); // Consome a citação de abertura
        while (peek() != '"' && peek() != '\0' && peek() != '\n') 
            nextchar();

        if (peek() == '"')
            nextchar(); // Consome a citação de fechamento
        else {
            fprintf(stderr, "String não fechada ou quebra de linha inesperada na string!\n");
            exit(1);
        }

        int len = POS - start; // O comprimento agora inclui ambas as aspas
        strncpy(tok.lexeme, SRC + start, len);
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
            nextchar();
            if (peek() == '&') { // &&
                nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "&&"); return tok;
            }
            fprintf(stderr, "Caractere inválido no lexer: '&' (apenas '&&' é suportado)\n");
            exit(1);
        case '|':
            nextchar();
            if (peek() == '|') { // ||
                nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "||"); return tok;
            }
            fprintf(stderr, "Caractere inválido no lexer: '|' (apenas '||' é suportado)\n");
            exit(1);

        // Operadores de 1 caractere
        case '+':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "+"); return tok;
        case '-':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "-"); return tok;
        case '*':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "*"); return tok;
        case '/':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "/"); return tok;
        case '>':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, ">"); return tok;
        case '<':
            nextchar(); tok.type = TOK_OPERATOR; strcpy(tok.lexeme, "<"); return tok;
    }

    fprintf(stderr, "Caractere inválido no lexer: '%c'\n", c);
    exit(1);
}