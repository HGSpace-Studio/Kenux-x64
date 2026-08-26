/*
 * PowerShell Script Engine - basic interpreter for the kernel.
 *
 * Implements tokenizing, parsing, and executing a small subset of
 * PowerShell-style scripts.  This is a deliberately simple interpreter
 * (no .NET integration, no full cmdlet set) suitable for running short
 * automation snippets inside the kernel.
 */

#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT* PDEVICE_OBJECT;
typedef struct _DRIVER_OBJECT* PDRIVER_OBJECT;
typedef struct _IRP* PIRP;
#endif

#include <arch/win32.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>

/* Kernel allocator (declared in <arch/memory.h>). */
extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* p);

/* ------------------------------------------------------------------ *
 * Static tables
 * ------------------------------------------------------------------ */

static const char* const g_ps_keywords[] = {
    "if", "else", "elseif", "foreach", "for", "while", "do", "function",
    "return", "param", "begin", "process", "end", "switch", "break",
    "continue", "throw", "try", "catch", "finally", "in", "filter",
    "class", "using", "hidden", "static", NULL
};

static const char* const g_ps_cmdlets[] = {
    "Write-Output", "Write-Host", "Get-Date", "Get-Process", "Get-Service",
    "Set-Variable", "Get-Variable", "Get-ChildItem", "Get-Content",
    "Set-Content", "Get-Location", "Set-Location", "New-Item",
    "Remove-Item", "Test-Path", "Start-Sleep", "Get-Random",
    "Get-Command", "Get-Help", NULL
};

/* ------------------------------------------------------------------ *
 * AST node pool (static, 256 nodes)
 * ------------------------------------------------------------------ */

static PSAST g_ast_pool[256];
static int   g_ast_pool_used = 0;

static PSAST* ps_ast_alloc(PSAST_TYPE type) {
    PSAST* node;
    if (g_ast_pool_used >= 256) {
        return NULL;
    }
    node = &g_ast_pool[g_ast_pool_used++];
    memset(node, 0, sizeof(PSAST));
    node->type = type;
    return node;
}

void ps_ast_free(PSAST* ast) {
    (void)ast;
    /* All AST nodes live in a static array; freeing just resets the
     * bump pointer so the pool can be reused by the next script. */
    g_ast_pool_used = 0;
}

/* ------------------------------------------------------------------ *
 * Keyword / cmdlet checks
 * ------------------------------------------------------------------ */

int ps_is_keyword(const char* word) {
    int i;
    if (!word) {
        return 0;
    }
    for (i = 0; g_ps_keywords[i] != NULL; i++) {
        if (strcmp(g_ps_keywords[i], word) == 0) {
            return 1;
        }
    }
    return 0;
}

int ps_is_builtin_cmdlet(const char* name) {
    int i;
    if (!name) {
        return 0;
    }
    for (i = 0; g_ps_cmdlets[i] != NULL; i++) {
        if (strcmp(g_ps_cmdlets[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 * Tokenizer
 * ------------------------------------------------------------------ */

static int ps_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static int ps_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int ps_is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

/* Identifiers may contain letters, digits, underscore, dash and dot so
 * that names like "Write-Output" or "Get-ChildItem" tokenize as a
 * single identifier. */
static int ps_is_ident_char(char c) {
    return ps_is_ident_start(c) ||
           ps_is_digit(c) ||
           c == '-' ||
           c == '.';
}

static void ps_set_text(char* dst, size_t dstsize, const char* s) {
    size_t n;
    if (!dst || dstsize == 0) {
        return;
    }
    if (!s) {
        dst[0] = '\0';
        return;
    }
    n = strlen(s);
    if (n >= dstsize) {
        n = dstsize - 1;
    }
    memcpy(dst, s, n);
    dst[n] = '\0';
}

int ps_tokenize(const char* script, PSTOKEN* tokens, int max_tokens) {
    int count = 0;
    int i = 0;
    int line = 1;

    if (!script || !tokens || max_tokens <= 0) {
        return 0;
    }

    while (script[i] != '\0' && count < max_tokens - 1) {
        char c = script[i];

        /* Whitespace (excluding newlines) */
        if (ps_is_space(c)) {
            i++;
            continue;
        }

        /* Newline */
        if (c == '\n') {
            tokens[count].type = PS_TOKEN_NEWLINE;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            line++;
            i++;
            continue;
        }

        /* Line comment */
        if (c == '#') {
            int j = 0;
            tokens[count].type = PS_TOKEN_COMMENT;
            tokens[count].line = line;
            while (script[i] != '\0' && script[i] != '\n') {
                if (j < (int)sizeof(tokens[count].text) - 1) {
                    tokens[count].text[j++] = script[i];
                }
                i++;
            }
            tokens[count].text[j] = '\0';
            count++;
            continue;
        }

        /* Variable: $name (the '$' is part of the token text) */
        if (c == '$') {
            int j = 0;
            tokens[count].type = PS_TOKEN_VARIABLE;
            tokens[count].line = line;
            tokens[count].text[j++] = c;
            i++;
            while (ps_is_ident_char(script[i])) {
                if (j < (int)sizeof(tokens[count].text) - 1) {
                    tokens[count].text[j++] = script[i];
                }
                i++;
            }
            tokens[count].text[j] = '\0';
            count++;
            continue;
        }

        /* Single-quoted string: surrounding quotes stripped */
        if (c == '\'') {
            int j = 0;
            tokens[count].type = PS_TOKEN_STRING;
            tokens[count].line = line;
            i++;
            while (script[i] != '\0' && script[i] != '\'') {
                if (j < (int)sizeof(tokens[count].text) - 1) {
                    tokens[count].text[j++] = script[i];
                }
                i++;
            }
            tokens[count].text[j] = '\0';
            if (script[i] == '\'') {
                i++;
            }
            count++;
            continue;
        }

        /* Double-quoted string: surrounding quotes stripped */
        if (c == '"') {
            int j = 0;
            tokens[count].type = PS_TOKEN_STRING;
            tokens[count].line = line;
            i++;
            while (script[i] != '\0' && script[i] != '"') {
                if (j < (int)sizeof(tokens[count].text) - 1) {
                    tokens[count].text[j++] = script[i];
                }
                i++;
            }
            tokens[count].text[j] = '\0';
            if (script[i] == '"') {
                i++;
            }
            count++;
            continue;
        }

        /* Number (integer or decimal) */
        if (ps_is_digit(c)) {
            int j = 0;
            tokens[count].type = PS_TOKEN_NUMBER;
            tokens[count].line = line;
            while (ps_is_digit(script[i]) || script[i] == '.') {
                if (j < (int)sizeof(tokens[count].text) - 1) {
                    tokens[count].text[j++] = script[i];
                }
                i++;
            }
            tokens[count].text[j] = '\0';
            count++;
            continue;
        }

        /* Identifier or keyword */
        if (ps_is_ident_start(c)) {
            int j = 0;
            tokens[count].line = line;
            while (ps_is_ident_char(script[i])) {
                if (j < (int)sizeof(tokens[count].text) - 1) {
                    tokens[count].text[j++] = script[i];
                }
                i++;
            }
            tokens[count].text[j] = '\0';
            tokens[count].type = ps_is_keyword(tokens[count].text)
                                     ? PS_TOKEN_KEYWORD
                                     : PS_TOKEN_IDENTIFIER;
            count++;
            continue;
        }

        /* Multi-char operators: ==, !=, <=, >= */
        if (c == '=' && script[i + 1] == '=') {
            tokens[count].type = PS_TOKEN_OPERATOR;
            tokens[count].line = line;
            ps_set_text(tokens[count].text, sizeof(tokens[count].text), "==");
            count++;
            i += 2;
            continue;
        }
        if (c == '!' && script[i + 1] == '=') {
            tokens[count].type = PS_TOKEN_OPERATOR;
            tokens[count].line = line;
            ps_set_text(tokens[count].text, sizeof(tokens[count].text), "!=");
            count++;
            i += 2;
            continue;
        }
        if ((c == '<' || c == '>') && script[i + 1] == '=') {
            tokens[count].type = PS_TOKEN_OPERATOR;
            tokens[count].line = line;
            tokens[count].text[0] = c;
            tokens[count].text[1] = '=';
            tokens[count].text[2] = '\0';
            count++;
            i += 2;
            continue;
        }

        /* Single-char operators: = + - * / < > */
        if (c == '=' || c == '+' || c == '-' ||
            c == '*' || c == '/' || c == '<' || c == '>') {
            tokens[count].type = PS_TOKEN_OPERATOR;
            tokens[count].line = line;
            tokens[count].text[0] = c;
            tokens[count].text[1] = '\0';
            count++;
            i++;
            continue;
        }

        /* Pipe */
        if (c == '|') {
            tokens[count].type = PS_TOKEN_PIPE;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            i++;
            continue;
        }

        /* Braces, parens, brackets */
        if (c == '{') {
            tokens[count].type = PS_TOKEN_LBRACE;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            i++;
            continue;
        }
        if (c == '}') {
            tokens[count].type = PS_TOKEN_RBRACE;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            i++;
            continue;
        }
        if (c == '(') {
            tokens[count].type = PS_TOKEN_LPAREN;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            i++;
            continue;
        }
        if (c == ')') {
            tokens[count].type = PS_TOKEN_RPAREN;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            i++;
            continue;
        }
        if (c == '[') {
            tokens[count].type = PS_TOKEN_LBRACKET;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            i++;
            continue;
        }
        if (c == ']') {
            tokens[count].type = PS_TOKEN_RBRACKET;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            i++;
            continue;
        }

        /* Semicolon, comma */
        if (c == ';') {
            tokens[count].type = PS_TOKEN_SEMICOLON;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            i++;
            continue;
        }
        if (c == ',') {
            tokens[count].type = PS_TOKEN_COMMA;
            tokens[count].line = line;
            tokens[count].text[0] = '\0';
            count++;
            i++;
            continue;
        }

        /* Anything else: skip silently */
        i++;
    }

    tokens[count].type = PS_TOKEN_EOF;
    tokens[count].line = line;
    tokens[count].text[0] = '\0';
    count++;

    return count;
}

/* ------------------------------------------------------------------ *
 * Parser
 * ------------------------------------------------------------------ */

typedef struct {
    PSTOKEN* tokens;
    int      count;
    int      pos;
} PSParser;

static PSTOKEN* ps_cur(PSParser* p) {
    if (p->pos >= p->count) {
        return NULL;
    }
    return &p->tokens[p->pos];
}

static void ps_advance(PSParser* p) {
    if (p->pos < p->count) {
        p->pos++;
    }
}

static int ps_match(PSParser* p, PSTOKEN_TYPE type) {
    PSTOKEN* t = ps_cur(p);
    if (t && t->type == type) {
        ps_advance(p);
        return 1;
    }
    return 0;
}

static void ps_skip_separators(PSParser* p) {
    while (ps_cur(p) &&
           (ps_cur(p)->type == PS_TOKEN_NEWLINE ||
            ps_cur(p)->type == PS_TOKEN_SEMICOLON)) {
        ps_advance(p);
    }
}

static void ps_skip_newlines(PSParser* p) {
    while (ps_cur(p) && ps_cur(p)->type == PS_TOKEN_NEWLINE) {
        ps_advance(p);
    }
}

static PSAST* ps_parse_statement(PSParser* p);
static PSAST* ps_parse_pipeline(PSParser* p);
static PSAST* ps_parse_expr(PSParser* p);
static PSAST* ps_parse_primary(PSParser* p);
static PSAST* ps_parse_block(PSParser* p);

static PSAST* ps_parse_block(PSParser* p) {
    PSAST* block;
    if (!ps_match(p, PS_TOKEN_LBRACE)) {
        return NULL;
    }
    block = ps_ast_alloc(PS_AST_SCRIPT);
    if (!block) {
        return NULL;
    }
    ps_skip_newlines(p);
    while (ps_cur(p) && ps_cur(p)->type != PS_TOKEN_RBRACE &&
           ps_cur(p)->type != PS_TOKEN_EOF) {
        PSAST* stmt = ps_parse_statement(p);
        if (stmt && block->num_children < 16) {
            block->children[block->num_children++] = stmt;
        }
        ps_skip_separators(p);
    }
    ps_match(p, PS_TOKEN_RBRACE);
    return block;
}

static PSAST* ps_parse_primary(PSParser* p) {
    PSTOKEN* t = ps_cur(p);
    PSAST* node;
    if (!t) {
        return NULL;
    }

    if (t->type == PS_TOKEN_STRING) {
        node = ps_ast_alloc(PS_AST_STRING);
        if (!node) return NULL;
        ps_set_text(node->text, sizeof(node->text), t->text);
        node->line = t->line;
        ps_advance(p);
        return node;
    }

    if (t->type == PS_TOKEN_NUMBER) {
        node = ps_ast_alloc(PS_AST_NUMBER);
        if (!node) return NULL;
        ps_set_text(node->text, sizeof(node->text), t->text);
        node->line = t->line;
        ps_advance(p);
        return node;
    }

    if (t->type == PS_TOKEN_VARIABLE) {
        node = ps_ast_alloc(PS_AST_VARIABLE);
        if (!node) return NULL;
        ps_set_text(node->text, sizeof(node->text), t->text);
        node->line = t->line;
        ps_advance(p);
        return node;
    }

    if (t->type == PS_TOKEN_LPAREN) {
        PSAST* inner;
        ps_advance(p);
        inner = ps_parse_pipeline(p);
        ps_match(p, PS_TOKEN_RPAREN);
        return inner;
    }

    /* Identifiers used as bare expressions become string literals
     * (rare in PowerShell but useful for arguments). */
    if (t->type == PS_TOKEN_IDENTIFIER) {
        node = ps_ast_alloc(PS_AST_STRING);
        if (!node) return NULL;
        ps_set_text(node->text, sizeof(node->text), t->text);
        node->line = t->line;
        ps_advance(p);
        return node;
    }

    return NULL;
}

static int ps_is_binary_op(const char* op) {
    if (!op) return 0;
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
        strcmp(op, "<")  == 0 || strcmp(op, ">")  == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
        strcmp(op, "+")  == 0 || strcmp(op, "-")  == 0 ||
        strcmp(op, "*")  == 0 || strcmp(op, "/")  == 0) {
        return 1;
    }
    return 0;
}

static PSAST* ps_parse_expr(PSParser* p) {
    PSAST* left = ps_parse_primary(p);
    if (!left) {
        return NULL;
    }
    while (ps_cur(p) && ps_cur(p)->type == PS_TOKEN_OPERATOR &&
           ps_is_binary_op(ps_cur(p)->text)) {
        PSAST* expr;
        PSAST* right;
        PSTOKEN* opt = ps_cur(p);
        ps_advance(p);
        right = ps_parse_primary(p);
        if (!right) {
            break;
        }
        expr = ps_ast_alloc(PS_AST_EXPRESSION);
        if (!expr) {
            break;
        }
        ps_set_text(expr->text, sizeof(expr->text), opt->text);
        expr->line = opt->line;
        if (expr->num_children < 16) {
            expr->children[expr->num_children++] = left;
        }
        if (expr->num_children < 16) {
            expr->children[expr->num_children++] = right;
        }
        left = expr;
    }
    return left;
}

static PSAST* ps_parse_command(PSParser* p) {
    PSTOKEN* t = ps_cur(p);
    PSAST* cmd;
    if (!t || (t->type != PS_TOKEN_IDENTIFIER && t->type != PS_TOKEN_KEYWORD)) {
        return NULL;
    }
    cmd = ps_ast_alloc(PS_AST_COMMAND);
    if (!cmd) {
        return NULL;
    }
    ps_set_text(cmd->text, sizeof(cmd->text), t->text);
    cmd->line = t->line;
    ps_advance(p);

    while (ps_cur(p)) {
        PSTOKEN_TYPE tt = ps_cur(p)->type;
        if (tt == PS_TOKEN_NEWLINE || tt == PS_TOKEN_SEMICOLON ||
            tt == PS_TOKEN_PIPE    || tt == PS_TOKEN_RBRACE    ||
            tt == PS_TOKEN_RPAREN  || tt == PS_TOKEN_EOF) {
            break;
        }
        {
            PSAST* arg = ps_parse_expr(p);
            if (!arg) {
                break;
            }
            if (cmd->num_children < 16) {
                cmd->children[cmd->num_children++] = arg;
            }
        }
    }
    return cmd;
}

static PSAST* ps_parse_pipeline(PSParser* p) {
    PSAST* first = ps_parse_command(p);
    if (!first) {
        first = ps_parse_expr(p);
        if (!first) {
            return NULL;
        }
    }

    /* Assignment: $var = value */
    if (first->type == PS_AST_VARIABLE && ps_cur(p) &&
        ps_cur(p)->type == PS_TOKEN_OPERATOR &&
        strcmp(ps_cur(p)->text, "=") == 0) {
        PSAST* assign;
        PSAST* value;
        ps_advance(p);
        value = ps_parse_pipeline(p);
        assign = ps_ast_alloc(PS_AST_ASSIGNMENT);
        if (!assign) {
            return first;
        }
        assign->line = first->line;
        if (assign->num_children < 16) {
            assign->children[assign->num_children++] = first;
        }
        if (value && assign->num_children < 16) {
            assign->children[assign->num_children++] = value;
        }
        return assign;
    }

    /* Pipeline: cmd1 | cmd2 | cmd3 */
    if (ps_cur(p) && ps_cur(p)->type == PS_TOKEN_PIPE) {
        PSAST* pipe = ps_ast_alloc(PS_AST_PIPELINE);
        if (!pipe) {
            return first;
        }
        if (pipe->num_children < 16) {
            pipe->children[pipe->num_children++] = first;
        }
        while (ps_cur(p) && ps_cur(p)->type == PS_TOKEN_PIPE) {
            PSAST* next;
            ps_advance(p);
            ps_skip_newlines(p);
            next = ps_parse_command(p);
            if (next && pipe->num_children < 16) {
                pipe->children[pipe->num_children++] = next;
            } else {
                break;
            }
        }
        return pipe;
    }

    return first;
}

static PSAST* ps_parse_if(PSParser* p) {
    PSAST* node = ps_ast_alloc(PS_AST_IF);
    PSAST* cond;
    PSAST* then_block;
    if (!node) {
        return NULL;
    }
    if (!ps_match(p, PS_TOKEN_LPAREN)) {
        return node;
    }
    cond = ps_parse_pipeline(p);
    ps_match(p, PS_TOKEN_RPAREN);
    if (cond && node->num_children < 16) {
        node->children[node->num_children++] = cond;
    }
    ps_skip_newlines(p);
    then_block = ps_parse_block(p);
    if (then_block && node->num_children < 16) {
        node->children[node->num_children++] = then_block;
    }

    /* elseif / else chain */
    ps_skip_newlines(p);
    while (ps_cur(p) && ps_cur(p)->type == PS_TOKEN_KEYWORD &&
           (strcmp(ps_cur(p)->text, "elseif") == 0 ||
            strcmp(ps_cur(p)->text, "else")   == 0)) {
        if (strcmp(ps_cur(p)->text, "elseif") == 0) {
            PSAST* nested;
            PSAST* eicond;
            PSAST* eiblock;
            ps_advance(p);
            if (!ps_match(p, PS_TOKEN_LPAREN)) {
                break;
            }
            eicond = ps_parse_pipeline(p);
            ps_match(p, PS_TOKEN_RPAREN);
            ps_skip_newlines(p);
            eiblock = ps_parse_block(p);
            nested = ps_ast_alloc(PS_AST_IF);
            if (nested) {
                if (eicond && nested->num_children < 16) {
                    nested->children[nested->num_children++] = eicond;
                }
                if (eiblock && nested->num_children < 16) {
                    nested->children[nested->num_children++] = eiblock;
                }
                if (node->num_children < 16) {
                    node->children[node->num_children++] = nested;
                }
            }
        } else {
            PSAST* elseblock;
            ps_advance(p);
            ps_skip_newlines(p);
            elseblock = ps_parse_block(p);
            if (elseblock && node->num_children < 16) {
                node->children[node->num_children++] = elseblock;
            }
            break;
        }
        ps_skip_newlines(p);
    }
    return node;
}

static PSAST* ps_parse_foreach(PSParser* p) {
    PSAST* node = ps_ast_alloc(PS_AST_FOREACH);
    PSTOKEN* var;
    PSAST* coll;
    PSAST* body;
    if (!node) {
        return NULL;
    }
    if (!ps_match(p, PS_TOKEN_LPAREN)) {
        return node;
    }
    var = ps_cur(p);
    if (var && var->type == PS_TOKEN_VARIABLE) {
        ps_set_text(node->text, sizeof(node->text), var->text);
        ps_advance(p);
    }
    if (ps_cur(p) && ps_cur(p)->type == PS_TOKEN_KEYWORD &&
        strcmp(ps_cur(p)->text, "in") == 0) {
        ps_advance(p);
    }
    coll = ps_parse_pipeline(p);
    if (coll && node->num_children < 16) {
        node->children[node->num_children++] = coll;
    }
    ps_match(p, PS_TOKEN_RPAREN);
    ps_skip_newlines(p);
    body = ps_parse_block(p);
    if (body && node->num_children < 16) {
        node->children[node->num_children++] = body;
    }
    return node;
}

static PSAST* ps_parse_while(PSParser* p) {
    PSAST* node = ps_ast_alloc(PS_AST_WHILE);
    PSAST* cond;
    PSAST* body;
    if (!node) {
        return NULL;
    }
    if (!ps_match(p, PS_TOKEN_LPAREN)) {
        return node;
    }
    cond = ps_parse_pipeline(p);
    ps_match(p, PS_TOKEN_RPAREN);
    if (cond && node->num_children < 16) {
        node->children[node->num_children++] = cond;
    }
    ps_skip_newlines(p);
    body = ps_parse_block(p);
    if (body && node->num_children < 16) {
        node->children[node->num_children++] = body;
    }
    return node;
}

static PSAST* ps_parse_for(PSParser* p) {
    PSAST* node = ps_ast_alloc(PS_AST_FOR);
    PSAST* init;
    PSAST* cond;
    PSAST* update;
    PSAST* body;
    if (!node) {
        return NULL;
    }
    if (!ps_match(p, PS_TOKEN_LPAREN)) {
        return node;
    }
    init = ps_parse_pipeline(p);
    ps_match(p, PS_TOKEN_SEMICOLON);
    cond = ps_parse_pipeline(p);
    ps_match(p, PS_TOKEN_SEMICOLON);
    update = ps_parse_pipeline(p);
    ps_match(p, PS_TOKEN_RPAREN);
    if (init   && node->num_children < 16) {
        node->children[node->num_children++] = init;
    }
    if (cond    && node->num_children < 16) {
        node->children[node->num_children++] = cond;
    }
    if (update  && node->num_children < 16) {
        node->children[node->num_children++] = update;
    }
    ps_skip_newlines(p);
    body = ps_parse_block(p);
    if (body && node->num_children < 16) {
        node->children[node->num_children++] = body;
    }
    return node;
}

static PSAST* ps_parse_function(PSParser* p) {
    PSAST* node = ps_ast_alloc(PS_AST_FUNCTION);
    PSTOKEN* name;
    PSAST* body;
    if (!node) {
        return NULL;
    }
    name = ps_cur(p);
    if (name && name->type == PS_TOKEN_IDENTIFIER) {
        ps_set_text(node->text, sizeof(node->text), name->text);
        ps_advance(p);
    }
    ps_skip_newlines(p);

    /* Optional parameter list in parentheses */
    if (ps_cur(p) && ps_cur(p)->type == PS_TOKEN_LPAREN) {
        ps_advance(p);
        while (ps_cur(p) && ps_cur(p)->type != PS_TOKEN_RPAREN &&
               ps_cur(p)->type != PS_TOKEN_EOF) {
            if (ps_cur(p)->type == PS_TOKEN_VARIABLE) {
                PSAST* param = ps_ast_alloc(PS_AST_PARAM);
                if (param) {
                    ps_set_text(param->text, sizeof(param->text), ps_cur(p)->text);
                    if (node->num_children < 16) {
                        node->children[node->num_children++] = param;
                    }
                }
                ps_advance(p);
            } else {
                ps_advance(p);
            }
            ps_match(p, PS_TOKEN_COMMA);
        }
        ps_match(p, PS_TOKEN_RPAREN);
        ps_skip_newlines(p);
    }

    body = ps_parse_block(p);
    if (body && node->num_children < 16) {
        node->children[node->num_children++] = body;
    }
    return node;
}

static PSAST* ps_parse_statement(PSParser* p) {
    PSTOKEN* t;
    ps_skip_separators(p);
    t = ps_cur(p);
    if (!t || t->type == PS_TOKEN_EOF) {
        return NULL;
    }

    if (t->type == PS_TOKEN_KEYWORD) {
        if (strcmp(t->text, "if") == 0) {
            ps_advance(p);
            return ps_parse_if(p);
        }
        if (strcmp(t->text, "foreach") == 0) {
            ps_advance(p);
            return ps_parse_foreach(p);
        }
        if (strcmp(t->text, "while") == 0) {
            ps_advance(p);
            return ps_parse_while(p);
        }
        if (strcmp(t->text, "for") == 0) {
            ps_advance(p);
            return ps_parse_for(p);
        }
        if (strcmp(t->text, "function") == 0) {
            ps_advance(p);
            return ps_parse_function(p);
        }
        /* Other keywords fall through and are treated as command names. */
    }

    return ps_parse_pipeline(p);
}

PSAST* ps_parse(PSTOKEN* tokens, int count) {
    PSParser p;
    PSAST* root;
    p.tokens = tokens;
    p.count  = count;
    p.pos    = 0;

    root = ps_ast_alloc(PS_AST_SCRIPT);
    if (!root) {
        return NULL;
    }

    ps_skip_separators(&p);
    while (p.pos < p.count) {
        PSTOKEN* t = ps_cur(&p);
        PSAST* stmt;
        if (!t || t->type == PS_TOKEN_EOF) {
            break;
        }
        if (t->type == PS_TOKEN_NEWLINE || t->type == PS_TOKEN_SEMICOLON) {
            ps_advance(&p);
            continue;
        }
        stmt = ps_parse_statement(&p);
        if (stmt) {
            if (root->num_children < 16) {
                root->children[root->num_children++] = stmt;
            } else {
                break;
            }
        } else {
            ps_advance(&p);
        }
        ps_skip_separators(&p);
    }
    return root;
}

/* ------------------------------------------------------------------ *
 * AST -> source serializer
 *
 * Used to capture the body of a function definition as text so that
 * it can be re-tokenized and re-executed each time the function is
 * called.  This is lossy for complex constructs but adequate for the
 * basic statements a "basic interpreter" is expected to handle.
 * ------------------------------------------------------------------ */

static void ps_ast_serialize(PSAST* node, char* buf, size_t bufsize) {
    int i;
    if (!buf || bufsize == 0) {
        return;
    }
    buf[0] = '\0';
    if (!node) {
        return;
    }

    switch (node->type) {
        case PS_AST_SCRIPT:
            for (i = 0; i < node->num_children; i++) {
                char tmp[1024];
                size_t cur;
                ps_ast_serialize(node->children[i], tmp, sizeof(tmp));
                cur = strlen(buf);
                if (cur + strlen(tmp) + 2 < bufsize) {
                    memcpy(buf + cur, tmp, strlen(tmp));
                    cur += strlen(tmp);
                    buf[cur++] = ';';
                    buf[cur++] = '\n';
                    buf[cur]   = '\0';
                }
            }
            break;

        case PS_AST_STRING: {
            int n = snprintf(buf, bufsize, "'%s'", node->text);
            if (n < 0 || (size_t)n >= bufsize) {
                buf[bufsize - 1] = '\0';
            }
            break;
        }

        case PS_AST_NUMBER:
        case PS_AST_VARIABLE:
            snprintf(buf, bufsize, "%s", node->text);
            break;

        case PS_AST_COMMAND: {
            size_t cur;
            snprintf(buf, bufsize, "%s", node->text);
            cur = strlen(buf);
            for (i = 0; i < node->num_children; i++) {
                char tmp[512];
                size_t tl;
                ps_ast_serialize(node->children[i], tmp, sizeof(tmp));
                tl = strlen(tmp);
                if (cur + tl + 2 < bufsize) {
                    buf[cur++] = ' ';
                    memcpy(buf + cur, tmp, tl);
                    cur += tl;
                    buf[cur] = '\0';
                }
            }
            break;
        }

        case PS_AST_PIPELINE:
            for (i = 0; i < node->num_children; i++) {
                char tmp[1024];
                size_t cur;
                size_t tl;
                ps_ast_serialize(node->children[i], tmp, sizeof(tmp));
                cur = strlen(buf);
                tl  = strlen(tmp);
                if (cur + tl + 4 < bufsize) {
                    if (i > 0) {
                        buf[cur++] = ' ';
                        buf[cur++] = '|';
                        buf[cur++] = ' ';
                    }
                    memcpy(buf + cur, tmp, tl);
                    cur += tl;
                    buf[cur] = '\0';
                }
            }
            break;

        case PS_AST_ASSIGNMENT: {
            char lhs[256];
            char rhs[1024];
            if (node->num_children >= 2) {
                ps_ast_serialize(node->children[0], lhs, sizeof(lhs));
                ps_ast_serialize(node->children[1], rhs, sizeof(rhs));
                snprintf(buf, bufsize, "%s = %s", lhs, rhs);
            }
            break;
        }

        case PS_AST_EXPRESSION: {
            char lhs[512];
            char rhs[512];
            if (node->num_children >= 2) {
                ps_ast_serialize(node->children[0], lhs, sizeof(lhs));
                ps_ast_serialize(node->children[1], rhs, sizeof(rhs));
                snprintf(buf, bufsize, "%s %s %s", lhs, node->text, rhs);
            }
            break;
        }

        case PS_AST_IF: {
            char cond[512];
            char then_b[1024];
            cond[0]   = '\0';
            then_b[0] = '\0';
            if (node->num_children >= 1) {
                ps_ast_serialize(node->children[0], cond, sizeof(cond));
            }
            if (node->num_children >= 2) {
                ps_ast_serialize(node->children[1], then_b, sizeof(then_b));
            }
            snprintf(buf, bufsize, "if (%s) { %s }", cond, then_b);
            break;
        }

        case PS_AST_FOREACH: {
            char coll[512];
            char body[1024];
            coll[0] = '\0';
            body[0] = '\0';
            if (node->num_children >= 1) {
                ps_ast_serialize(node->children[0], coll, sizeof(coll));
            }
            if (node->num_children >= 2) {
                ps_ast_serialize(node->children[1], body, sizeof(body));
            }
            snprintf(buf, bufsize, "foreach (%s in %s) { %s }",
                     node->text, coll, body);
            break;
        }

        case PS_AST_WHILE: {
            char cond[512];
            char body[1024];
            cond[0] = '\0';
            body[0] = '\0';
            if (node->num_children >= 1) {
                ps_ast_serialize(node->children[0], cond, sizeof(cond));
            }
            if (node->num_children >= 2) {
                ps_ast_serialize(node->children[1], body, sizeof(body));
            }
            snprintf(buf, bufsize, "while (%s) { %s }", cond, body);
            break;
        }

        case PS_AST_FOR: {
            char init[256];
            char cond[256];
            char upd[256];
            char body[1024];
            init[0] = cond[0] = upd[0] = body[0] = '\0';
            if (node->num_children >= 1) {
                ps_ast_serialize(node->children[0], init, sizeof(init));
            }
            if (node->num_children >= 2) {
                ps_ast_serialize(node->children[1], cond, sizeof(cond));
            }
            if (node->num_children >= 3) {
                ps_ast_serialize(node->children[2], upd, sizeof(upd));
            }
            if (node->num_children >= 4) {
                ps_ast_serialize(node->children[3], body, sizeof(body));
            }
            snprintf(buf, bufsize, "for (%s; %s; %s) { %s }",
                     init, cond, upd, body);
            break;
        }

        default:
            break;
    }
}

/* ------------------------------------------------------------------ *
 * Variable management
 * ------------------------------------------------------------------ */

void ps_set_variable(PSENGINE* engine, const char* name, VARIANT* val) {
    PSSCOPE* scope;
    int i;
    if (!engine || !name || !val) {
        return;
    }
    scope = &engine->global_scope;

    /* Update existing variable if present. */
    for (i = 0; i < scope->num_vars; i++) {
        if (strcmp(scope->variables[i].name, name) == 0) {
            if (scope->variables[i].value.vt == VT_BSTR &&
                scope->variables[i].value.byref) {
                memory_free(scope->variables[i].value.byref);
            }
            scope->variables[i].value = *val;
            if (val->vt == VT_BSTR && val->byref) {
                char* copy = (char*)memory_alloc(strlen((char*)val->byref) + 1);
                if (copy) {
                    strcpy(copy, (char*)val->byref);
                    scope->variables[i].value.byref = copy;
                }
            }
            scope->variables[i].is_set = 1;
            return;
        }
    }

    /* Create new variable. */
    if (scope->num_vars >= 256) {
        return;
    }
    {
        PSVAR* v = &scope->variables[scope->num_vars++];
        memset(v, 0, sizeof(PSVAR));
        snprintf(v->name, sizeof(v->name), "%s", name);
        v->value = *val;
        if (val->vt == VT_BSTR && val->byref) {
            char* copy = (char*)memory_alloc(strlen((char*)val->byref) + 1);
            if (copy) {
                strcpy(copy, (char*)val->byref);
                v->value.byref = copy;
            }
        }
        v->is_set = 1;
    }
}

VARIANT* ps_get_variable(PSENGINE* engine, const char* name) {
    PSSCOPE* scope;
    int i;
    if (!engine || !name) {
        return NULL;
    }
    scope = &engine->global_scope;
    while (scope) {
        for (i = 0; i < scope->num_vars; i++) {
            if (strcmp(scope->variables[i].name, name) == 0) {
                return &scope->variables[i].value;
            }
        }
        scope = scope->parent;
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * Function management
 * ------------------------------------------------------------------ */

int ps_register_function(PSENGINE* engine, const char* name,
                         const char* body, const char* params[], int nparams) {
    int i;
    int np;
    PSFUNC* f;
    if (!engine || !name) {
        return 0;
    }
    np = nparams > 8 ? 8 : nparams;
    if (np < 0) {
        np = 0;
    }

    /* Update existing function. */
    for (i = 0; i < engine->num_functions; i++) {
        if (engine->functions[i].defined &&
            strcmp(engine->functions[i].name, name) == 0) {
            int j;
            if (body) {
                snprintf(engine->functions[i].body,
                         sizeof(engine->functions[i].body), "%s", body);
            } else {
                engine->functions[i].body[0] = '\0';
            }
            engine->functions[i].num_params = np;
            for (j = 0; j < np; j++) {
                if (params && params[j]) {
                    snprintf(engine->functions[i].params[j],
                             sizeof(engine->functions[i].params[j]),
                             "%s", params[j]);
                } else {
                    engine->functions[i].params[j][0] = '\0';
                }
            }
            return 1;
        }
    }

    if (engine->num_functions >= 64) {
        return 0;
    }
    f = &engine->functions[engine->num_functions++];
    memset(f, 0, sizeof(PSFUNC));
    snprintf(f->name, sizeof(f->name), "%s", name);
    if (body) {
        snprintf(f->body, sizeof(f->body), "%s", body);
    }
    f->num_params = np;
    for (i = 0; i < np; i++) {
        if (params && params[i]) {
            snprintf(f->params[i], sizeof(f->params[i]), "%s", params[i]);
        }
    }
    f->defined = 1;
    return 1;
}

static PSFUNC* ps_find_function(PSENGINE* engine, const char* name) {
    int i;
    if (!engine || !name) {
        return NULL;
    }
    for (i = 0; i < engine->num_functions; i++) {
        if (engine->functions[i].defined &&
            strcmp(engine->functions[i].name, name) == 0) {
            return &engine->functions[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * VARIANT helpers
 * ------------------------------------------------------------------ */

static void ps_variant_set_str(VARIANT* v, const char* s) {
    char* copy;
    size_t len;
    if (!v || !s) {
        return;
    }
    memset(v, 0, sizeof(VARIANT));
    v->vt = VT_BSTR;
    len = strlen(s) + 1;
    copy = (char*)memory_alloc((uint64_t)len);
    if (copy) {
        memcpy(copy, s, len);
        v->byref = copy;
    }
}

static void ps_variant_set_long(VARIANT* v, LONG val) {
    if (!v) {
        return;
    }
    memset(v, 0, sizeof(VARIANT));
    v->vt = VT_I4;
    v->lVal = val;
}

static void ps_variant_set_bool(VARIANT* v, int b) {
    if (!v) {
        return;
    }
    memset(v, 0, sizeof(VARIANT));
    v->vt = VT_BOOL;
    v->boolVal = b ? -1 : 0; /* VARIANT_TRUE == -1 */
}

static LONG ps_variant_to_long(VARIANT* v) {
    const char* s;
    LONG val;
    int neg;
    if (!v) {
        return 0;
    }
    if (v->vt == VT_I4) {
        return v->lVal;
    }
    if (v->vt == VT_BOOL) {
        return v->boolVal != 0 ? 1 : 0;
    }
    if (v->vt == VT_BSTR) {
        s = (const char*)v->byref;
        if (!s) {
            return 0;
        }
        val = 0;
        neg = 0;
        if (*s == '-') {
            neg = 1;
            s++;
        }
        while (*s >= '0' && *s <= '9') {
            val = val * 10 + (*s - '0');
            s++;
        }
        return neg ? -val : val;
    }
    return 0;
}

static void ps_variant_to_str(VARIANT* v, char* buf, size_t bufsize) {
    if (!buf || bufsize == 0) {
        return;
    }
    buf[0] = '\0';
    if (!v) {
        return;
    }
    if (v->vt == VT_BSTR) {
        const char* s = (const char*)v->byref;
        if (s) {
            snprintf(buf, bufsize, "%s", s);
        }
    } else if (v->vt == VT_I4) {
        snprintf(buf, bufsize, "%ld", (long)v->lVal);
    } else if (v->vt == VT_BOOL) {
        snprintf(buf, bufsize, "%s", v->boolVal ? "True" : "False");
    }
}

/* ------------------------------------------------------------------ *
 * PSRESULT helpers
 * ------------------------------------------------------------------ */

static void ps_result_set_str(PSRESULT* r, const char* s) {
    if (!r) {
        return;
    }
    memset(r, 0, sizeof(PSRESULT));
    ps_variant_set_str(&r->value, s);
    r->has_result = 1;
}

static void ps_result_set_long(PSRESULT* r, LONG val) {
    if (!r) {
        return;
    }
    memset(r, 0, sizeof(PSRESULT));
    ps_variant_set_long(&r->value, val);
    r->has_result = 1;
}

static void ps_result_set_bool(PSRESULT* r, int b) {
    if (!r) {
        return;
    }
    memset(r, 0, sizeof(PSRESULT));
    ps_variant_set_bool(&r->value, b);
    r->has_result = 1;
}

/* ------------------------------------------------------------------ *
 * Executor
 * ------------------------------------------------------------------ */

static int ps_exec_stmt(PSENGINE* engine, PSAST* node, PSRESULT* result);

static int ps_eval_condition(PSENGINE* engine, PSAST* node, PSRESULT* result) {
    if (!ps_exec_stmt(engine, node, result)) {
        return 0;
    }
    if (!result->has_result) {
        return 0;
    }
    if (result->value.vt == VT_BOOL) {
        return result->value.boolVal != 0;
    }
    if (result->value.vt == VT_I4) {
        return result->value.lVal != 0;
    }
    if (result->value.vt == VT_BSTR) {
        const char* s = (const char*)result->value.byref;
        if (!s) {
            return 0;
        }
        if (s[0] == '\0') {
            return 0;
        }
        if (strcmp(s, "true") == 0 || strcmp(s, "True") == 0 ||
            strcmp(s, "1")    == 0) {
            return 1;
        }
        if (strcmp(s, "false") == 0 || strcmp(s, "False") == 0 ||
            strcmp(s, "0")     == 0) {
            return 0;
        }
        return 1;
    }
    return 0;
}

static int ps_exec_command(PSENGINE* engine, PSAST* node, PSRESULT* result) {
    int i;
    int argc;
    PSRESULT args[16];
    PSFUNC* func;
    PSTOKEN tokens[256];
    int ntok;
    PSAST* body_ast;

    if (!engine || !node || !result) {
        return 0;
    }

    /* Built-in cmdlets handled inline. */
    if (strcmp(node->text, "Write-Output") == 0 ||
        strcmp(node->text, "Write-Host")   == 0) {
        if (node->num_children > 0) {
            return ps_exec_stmt(engine, node->children[0], result);
        }
        ps_result_set_str(result, "");
        return 1;
    }
    if (strcmp(node->text, "Get-Date") == 0) {
        ps_result_set_str(result, "01/01/2026 00:00:00");
        return 1;
    }
    if (strcmp(node->text, "Get-Random") == 0) {
        ps_result_set_long(result, 42);
        return 1;
    }
    if (strcmp(node->text, "Get-Process") == 0) {
        ps_result_set_str(result, "System.Diagnostics.Process");
        return 1;
    }
    if (strcmp(node->text, "Get-Service") == 0) {
        ps_result_set_str(result, "System.ServiceProcess.ServiceController");
        return 1;
    }
    if (strcmp(node->text, "Get-ChildItem") == 0) {
        ps_result_set_str(result, "");
        return 1;
    }
    if (strcmp(node->text, "Get-Content") == 0) {
        ps_result_set_str(result, "");
        return 1;
    }
    if (strcmp(node->text, "Set-Content") == 0) {
        ps_result_set_str(result, "");
        return 1;
    }
    if (strcmp(node->text, "Get-Location") == 0) {
        ps_result_set_str(result, "C:\\");
        return 1;
    }
    if (strcmp(node->text, "Set-Location") == 0) {
        ps_result_set_str(result, "");
        return 1;
    }
    if (strcmp(node->text, "New-Item") == 0) {
        ps_result_set_str(result, "");
        return 1;
    }
    if (strcmp(node->text, "Remove-Item") == 0) {
        ps_result_set_str(result, "");
        return 1;
    }
    if (strcmp(node->text, "Test-Path") == 0) {
        ps_result_set_bool(result, 0);
        return 1;
    }
    if (strcmp(node->text, "Start-Sleep") == 0) {
        /* No real sleep in this minimal interpreter. */
        ps_result_set_str(result, "");
        return 1;
    }
    if (strcmp(node->text, "Get-Command") == 0 ||
        strcmp(node->text, "Get-Help")    == 0 ||
        strcmp(node->text, "Get-Variable") == 0 ||
        strcmp(node->text, "Set-Variable") == 0) {
        ps_result_set_str(result, "");
        return 1;
    }

    /* User-defined function. */
    func = ps_find_function(engine, node->text);
    if (!func) {
        /* Unknown command: return its name as a string result. */
        ps_result_set_str(result, node->text);
        return 0;
    }

    /* Evaluate arguments. */
    argc = node->num_children;
    if (argc > 16) {
        argc = 16;
    }
    for (i = 0; i < argc; i++) {
        memset(&args[i], 0, sizeof(PSRESULT));
        ps_exec_stmt(engine, node->children[i], &args[i]);
    }

    /* Bind parameters to variables in the global scope. */
    for (i = 0; i < func->num_params && i < argc; i++) {
        ps_set_variable(engine, func->params[i], &args[i].value);
    }

    /* Execute the function body. */
    if (func->body[0] != '\0') {
        ntok = ps_tokenize(func->body, tokens, 256);
        body_ast = ps_parse(tokens, ntok);
        if (body_ast) {
            ps_exec_stmt(engine, body_ast, result);
            ps_ast_free(body_ast);
        }
    } else {
        ps_result_set_str(result, "");
    }
    return 1;
}

static int ps_exec_stmt(PSENGINE* engine, PSAST* node, PSRESULT* result) {
    int i;
    if (!engine || !node || !result) {
        return 0;
    }

    switch (node->type) {
        case PS_AST_SCRIPT: {
            int rc = 0;
            for (i = 0; i < node->num_children; i++) {
                rc = ps_exec_stmt(engine, node->children[i], result);
            }
            return rc;
        }

        case PS_AST_COMMAND:
            return ps_exec_command(engine, node, result);

        case PS_AST_PIPELINE: {
            int rc = 0;
            for (i = 0; i < node->num_children; i++) {
                /* Simplified pipeline: each stage runs and its result
                 * becomes the input to the next stage.  Without real
                 * pipeline variable ($_) support, we just pass the
                 * result through. */
                rc = ps_exec_command(engine, node->children[i], result);
            }
            return rc;
        }

        case PS_AST_ASSIGNMENT: {
            PSAST* var;
            PSAST* val;
            PSRESULT vres;
            if (node->num_children < 2) {
                return 0;
            }
            var = node->children[0];
            val = node->children[1];
            memset(&vres, 0, sizeof(PSRESULT));
            ps_exec_stmt(engine, val, &vres);
            if (var->type == PS_AST_VARIABLE) {
                ps_set_variable(engine, var->text, &vres.value);
                *result = vres;
            }
            return 1;
        }

        case PS_AST_IF: {
            PSRESULT cond_res;
            if (node->num_children < 1) {
                return 0;
            }
            memset(&cond_res, 0, sizeof(PSRESULT));
            if (ps_eval_condition(engine, node->children[0], &cond_res)) {
                if (node->num_children >= 2) {
                    return ps_exec_stmt(engine, node->children[1], result);
                }
                return 1;
            }
            /* Walk the elseif/else chain. */
            for (i = 2; i < node->num_children; i++) {
                PSAST* branch = node->children[i];
                if (branch->type == PS_AST_IF) {
                    PSRESULT cres;
                    memset(&cres, 0, sizeof(PSRESULT));
                    if (branch->num_children >= 1 &&
                        ps_eval_condition(engine, branch->children[0], &cres)) {
                        if (branch->num_children >= 2) {
                            return ps_exec_stmt(engine, branch->children[1], result);
                        }
                        return 1;
                    }
                } else {
                    /* else block. */
                    return ps_exec_stmt(engine, branch, result);
                }
            }
            return 1;
        }

        case PS_AST_FOREACH: {
            PSAST* coll;
            PSAST* body;
            PSRESULT cres;
            if (node->num_children < 2) {
                return 0;
            }
            coll = node->children[0];
            body = node->children[1];
            memset(&cres, 0, sizeof(PSRESULT));
            ps_exec_stmt(engine, coll, &cres);
            if (cres.has_result) {
                ps_set_variable(engine, node->text, &cres.value);
                ps_exec_stmt(engine, body, result);
            }
            return 1;
        }

        case PS_AST_WHILE: {
            int iterations = 0;
            PSRESULT cres;
            if (node->num_children < 2) {
                return 0;
            }
            while (iterations < 1000) {
                memset(&cres, 0, sizeof(PSRESULT));
                if (!ps_eval_condition(engine, node->children[0], &cres)) {
                    break;
                }
                ps_exec_stmt(engine, node->children[1], result);
                iterations++;
            }
            return 1;
        }

        case PS_AST_FOR: {
            int iterations = 0;
            PSRESULT cres;
            if (node->num_children < 4) {
                return 0;
            }
            ps_exec_stmt(engine, node->children[0], result); /* init */
            while (iterations < 1000) {
                memset(&cres, 0, sizeof(PSRESULT));
                if (!ps_eval_condition(engine, node->children[1], &cres)) {
                    break;
                }
                ps_exec_stmt(engine, node->children[3], result); /* body */
                ps_exec_stmt(engine, node->children[2], result); /* update */
                iterations++;
            }
            return 1;
        }

        case PS_AST_FUNCTION: {
            const char* params[8];
            int nparams = 0;
            PSAST* body = NULL;
            char body_text[4096];
            for (i = 0; i < node->num_children; i++) {
                if (node->children[i]->type == PS_AST_PARAM) {
                    if (nparams < 8) {
                        params[nparams++] = node->children[i]->text;
                    }
                } else if (node->children[i]->type == PS_AST_SCRIPT) {
                    body = node->children[i];
                }
            }
            body_text[0] = '\0';
            if (body) {
                ps_ast_serialize(body, body_text, sizeof(body_text));
            }
            ps_register_function(engine, node->text, body_text, params, nparams);
            ps_result_set_str(result, "");
            return 1;
        }

        case PS_AST_VARIABLE: {
            VARIANT* v = ps_get_variable(engine, node->text);
            memset(result, 0, sizeof(PSRESULT));
            if (v) {
                result->value = *v;
                /* If string, copy so caller can free safely. */
                if (v->vt == VT_BSTR && v->byref) {
                    ps_variant_set_str(&result->value, (const char*)v->byref);
                }
                result->has_result = 1;
            } else {
                ps_variant_set_str(&result->value, "");
                result->has_result = 1;
            }
            return 1;
        }

        case PS_AST_STRING:
            ps_result_set_str(result, node->text);
            return 1;

        case PS_AST_NUMBER: {
            LONG val = 0;
            const char* s = node->text;
            int neg = 0;
            if (*s == '-') {
                neg = 1;
                s++;
            }
            while (*s >= '0' && *s <= '9') {
                val = val * 10 + (*s - '0');
                s++;
            }
            if (neg) {
                val = -val;
            }
            ps_result_set_long(result, val);
            return 1;
        }

        case PS_AST_EXPRESSION: {
            PSRESULT lres;
            PSRESULT rres;
            const char* op;
            if (node->num_children < 2) {
                return 0;
            }
            memset(&lres, 0, sizeof(PSRESULT));
            memset(&rres, 0, sizeof(PSRESULT));
            ps_exec_stmt(engine, node->children[0], &lres);
            ps_exec_stmt(engine, node->children[1], &rres);
            op = node->text;

            if (strcmp(op, "+") == 0) {
                if (lres.value.vt == VT_I4 && rres.value.vt == VT_I4) {
                    ps_result_set_long(result, lres.value.lVal + rres.value.lVal);
                } else {
                    char lstr[256];
                    char rstr[256];
                    char buf[512];
                    ps_variant_to_str(&lres.value, lstr, sizeof(lstr));
                    ps_variant_to_str(&rres.value, rstr, sizeof(rstr));
                    snprintf(buf, sizeof(buf), "%s%s", lstr, rstr);
                    ps_result_set_str(result, buf);
                }
                return 1;
            }
            if (strcmp(op, "-") == 0) {
                LONG l = ps_variant_to_long(&lres.value);
                LONG r = ps_variant_to_long(&rres.value);
                ps_result_set_long(result, l - r);
                return 1;
            }
            if (strcmp(op, "*") == 0) {
                LONG l = ps_variant_to_long(&lres.value);
                LONG r = ps_variant_to_long(&rres.value);
                ps_result_set_long(result, l * r);
                return 1;
            }
            if (strcmp(op, "/") == 0) {
                LONG l = ps_variant_to_long(&lres.value);
                LONG r = ps_variant_to_long(&rres.value);
                ps_result_set_long(result, r != 0 ? l / r : 0);
                return 1;
            }

            /* Comparisons. */
            if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
                strcmp(op, "<")  == 0 || strcmp(op, ">")  == 0 ||
                strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
                int cmp;
                int truth = 0;
                if (lres.value.vt == VT_I4 && rres.value.vt == VT_I4) {
                    if (lres.value.lVal == rres.value.lVal) {
                        cmp = 0;
                    } else if (lres.value.lVal < rres.value.lVal) {
                        cmp = -1;
                    } else {
                        cmp = 1;
                    }
                } else {
                    char lstr[256];
                    char rstr[256];
                    ps_variant_to_str(&lres.value, lstr, sizeof(lstr));
                    ps_variant_to_str(&rres.value, rstr, sizeof(rstr));
                    cmp = strcmp(lstr, rstr);
                }
                if (strcmp(op, "==") == 0) {
                    truth = (cmp == 0);
                } else if (strcmp(op, "!=") == 0) {
                    truth = (cmp != 0);
                } else if (strcmp(op, "<") == 0) {
                    truth = (cmp < 0);
                } else if (strcmp(op, ">") == 0) {
                    truth = (cmp > 0);
                } else if (strcmp(op, "<=") == 0) {
                    truth = (cmp <= 0);
                } else if (strcmp(op, ">=") == 0) {
                    truth = (cmp >= 0);
                }
                ps_result_set_bool(result, truth);
                return 1;
            }
            return 0;
        }

        case PS_AST_PARAM:
            /* Param markers are not executable. */
            ps_result_set_str(result, node->text);
            return 1;

        case PS_AST_ARRAY:
        case PS_AST_HASHTABLE:
            /* Not implemented in this basic interpreter. */
            ps_result_set_str(result, "");
            return 1;

        default:
            ps_result_set_str(result, "");
            return 0;
    }
}

int ps_execute(PSENGINE* engine, PSAST* ast, PSRESULT* result) {
    if (!engine || !ast || !result) {
        return 0;
    }
    memset(result, 0, sizeof(PSRESULT));
    return ps_exec_stmt(engine, ast, result);
}

/* ------------------------------------------------------------------ *
 * Engine lifecycle
 * ------------------------------------------------------------------ */

void ps_engine_init(PSENGINE* engine) {
    if (!engine) {
        return;
    }
    memset(engine, 0, sizeof(PSENGINE));
    engine->global_scope.parent = NULL;
}

void ps_engine_cleanup(PSENGINE* engine) {
    int i;
    if (!engine) {
        return;
    }
    for (i = 0; i < engine->global_scope.num_vars; i++) {
        if (engine->global_scope.variables[i].value.vt == VT_BSTR &&
            engine->global_scope.variables[i].value.byref) {
            memory_free(engine->global_scope.variables[i].value.byref);
        }
    }
    memset(engine, 0, sizeof(PSENGINE));
}

const char* ps_get_error(PSENGINE* engine) {
    if (!engine || !engine->has_error) {
        return "";
    }
    return engine->error_msg;
}

static void ps_set_error(PSENGINE* engine, const char* msg, int line) {
    if (!engine || !msg) {
        return;
    }
    snprintf(engine->error_msg, sizeof(engine->error_msg), "%s (line %d)", msg, line);
    engine->has_error = 1;
    engine->error_line = line;
}

/* ------------------------------------------------------------------ *
 * Built-in cmdlet registration
 * ------------------------------------------------------------------ */

void ps_register_builtins(PSENGINE* engine) {
    static const char* empty_params[1] = {NULL};
    int i;
    if (!engine) {
        return;
    }
    for (i = 0; g_ps_cmdlets[i] != NULL; i++) {
        ps_register_function(engine, g_ps_cmdlets[i], "", empty_params, 0);
    }
}

/* ------------------------------------------------------------------ *
 * Top-level run helpers
 * ------------------------------------------------------------------ */

int ps_run_script(PSENGINE* engine, const char* script, PSRESULT* result) {
    PSTOKEN tokens[256];
    int ntok;
    PSAST* ast;

    if (!engine || !script || !result) {
        return 0;
    }

    ntok = ps_tokenize(script, tokens, 256);
    ast = ps_parse(tokens, ntok);
    if (!ast) {
        ps_set_error(engine, "Parse error", 0);
        memset(result, 0, sizeof(PSRESULT));
        result->is_error = 1;
        snprintf(result->error_msg, sizeof(result->error_msg), "Parse error");
        return 0;
    }

    memset(result, 0, sizeof(PSRESULT));
    {
        int rc = ps_execute(engine, ast, result);
        ps_ast_free(ast);
        return rc;
    }
}

int ps_run_command(PSENGINE* engine, const char* cmd, PSRESULT* result) {
    /* A single command is just a one-line script. */
    return ps_run_script(engine, cmd, result);
}

/* ------------------------------------------------------------------ *
 * Global engine entry point
 * ------------------------------------------------------------------ */

static PSENGINE g_ps_engine;

int powershell_init(void) {
    ps_engine_init(&g_ps_engine);
    ps_register_builtins(&g_ps_engine);
    return 0;
}
