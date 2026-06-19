#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>
#include <assert.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "linkedList.h"

void srcCodeLine_delete(void* data);
void macros_delete(void* data);
void tokenStr_delete(void* data);
void labels_delete(void* data);
void statement_delete(void* data);

struct srcCodeLine {
    char* str;
    size_t str_len;
    size_t line;
    char* file;
};

struct macro {
    char* macroName;
    char* replacement;
};

struct label {
    char* name;
    uint16_t addr;
};

struct tok {
    char* tok;
    char* src;
};

/*
 * A = addr
 * S = src
 * D = dest
 * P = port id
 * I = imm8
 * N = nothing
 */

enum ins_pattern {
    PAT_A,
    PAT_N,
    PAT_S,
    PAT_D,
    PAT_SP,
    PAT_DP,
    PAT_DS,
    PAT_DSS,
    PAT_DI,
    PAT_SI,
    PAT_SDI,
};

struct instruction {
    enum ins_pattern pat;
    uint8_t opcode;
    uint16_t addr;
    char* addr_label; // Optional, null if unused
    uint8_t imm8;
    uint8_t pid;
    uint8_t dest;
    uint8_t src1;
    uint8_t src2;
};

enum statementType {
    STATEMENT_CPUINSTRUCTION,
    STATEMENT_DIRECTIVE_OFFSET,
    STATEMENT_DIRECTIVE_ASCIIZ,
};

struct statement {
    enum statementType type;
    union {
        struct instruction instr;
        uint16_t offsetAddr;
        struct {
            char* buf;
            size_t size;
        } asciiz;
    } data;
};

char* stripTrailingSlash(char *path) {
    size_t len = strlen(path);
    char* str = strdup(path);
    while (len > 1 && str[len - 1] == '/') {
        str[len - 1] = '\0';
        len--;
    }
    return str;
}

char* resolveRelPath(const char* base, bool baseIsDir, const char* rel) {
    assert(base != NULL && rel != NULL);
    assert(rel[0] != '/');
    char* basec = strdup(base);
    char* relc = strdup(rel);

    char* dir = NULL;
    if(baseIsDir) dir = stripTrailingSlash(basec);
    else dir = strdup(dirname(basec));
    char* final = calloc(strlen(dir) + strlen(rel) + 2, sizeof(char));
    strcpy(final, dir);
    strcat(final, "/");
    strcat(final, rel);
    free(basec);
    free(relc);
    free(dir);
    return final;
}

char* resolveInIncludes(const char* file, const struct ll_head* includes) {
    for(struct ll_node* cur = includes->start; cur != NULL; cur = cur->next) {
        char* tmp = resolveRelPath(cur->data, true, file);
        if(access(tmp, R_OK) == 0) {
            return tmp;
        }
        printf("check on \"%s\" failed\n", tmp);
    }
    return NULL;
}

char* stripWhitespace(const char* str, size_t len) {
    char* wsStripped = calloc(len + 1, sizeof(char));
    size_t wssIdx = 0;

    bool inQuote = false;

    for(size_t i = 0; i < len; i++) {
        if((str[i] != ' ' && str[i] != '\t') || inQuote) {
            wsStripped[++wssIdx - 1] = str[i];
            if(str[i] == '\"') inQuote = !inQuote;
            continue;
        } else {
            if(wsStripped[wssIdx - 1] == ' ') {
                continue;
            } else {
                wsStripped[++wssIdx - 1] = ' ';
            }
        }
    }
    if(wsStripped[wssIdx - 1] == ' ') wsStripped[wssIdx-- -1] = '\0';
    if(wsStripped[0] == ' ') {
        char* tmp = strdup(&wsStripped[1]);
        free(wsStripped);
        wsStripped = tmp;
    }
    return wsStripped;
}

#define RESET   "\033[0m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

/*
(DONE) #include

#embed

(DONE) #define
(DONE) #undef

(DONE) #ifdef
(DONE) #ifndef
(maybe not?) #else
(DONE) #endif

How to macro expansions:
macro name = uninterrupted sequnce of a-z, A-Z, 0-9, or "_"  CANT start with digit
sequence is interrupted by: ' ', '\t', ',', '\"'      also outside quote
*/

#define VALID_MACRO_NAME_CHAR(c) \
    ((((c) >= '0' && (c) <= '9') || \
      ((c) >= 'a' && (c) <= 'z') || \
      ((c) >= 'A' && (c) <= 'Z') || \
      (c) == '_'))

void macroRepl_del(void* ptr) {free(ptr);}

const char* findMacro(struct ll_head* macros, const char* name) {
    struct ll_node* cur = macros->start;

    while (cur != NULL) {
        struct macro* mac = cur->data;

        if (strcmp(mac->macroName, name) == 0)
            return mac->replacement;

        cur = cur->next;
    }

    return NULL;
}

char* expandMacros(const char* input, struct ll_head* macros) {
    size_t cap = 256;
    size_t outLen = 0;

    char* out = malloc(cap);

    size_t i = 0;

    int atLineStart = 1;
    int inDirective = 0;
    int suppressNextIdentifier = 0;

    while (input[i]) {

        // detect start of preprocessor directive
        if (atLineStart && input[i] == '#') {
            inDirective = 1;
        }

        // valid identifier start
        if (isalpha((unsigned char)input[i]) ||
            input[i] == '_') {

            size_t start = i;

            i++;

            // identifier continuation
            while (isalnum((unsigned char)input[i]) ||
                   input[i] == '_') {
                i++;
            }

            size_t len = i - start;

            char ident[256];

            memcpy(ident, input + start, len);
            ident[len] = '\0';

            const char* text;

            if (inDirective) {

                // directives where next identifier
                // must NOT be expanded
                if (strcmp(ident, "define") == 0 ||
                    strcmp(ident, "undef") == 0 ||
                    strcmp(ident, "ifdef") == 0 ||
                    strcmp(ident, "ifndef") == 0) {

                    suppressNextIdentifier = 1;
                    text = ident;
                }
                else if (suppressNextIdentifier) {

                    suppressNextIdentifier = 0;
                    text = ident;
                }
                else {

                    const char* repl =
                        findMacro(macros, ident);

                    text = repl ? repl : ident;
                }
            }
            else {

                const char* repl =
                    findMacro(macros, ident);

                text = repl ? repl : ident;
            }

            size_t textLen = strlen(text);

            // grow output buffer
            while (outLen + textLen + 1 > cap) {
                cap *= 2;
                out = realloc(out, cap);
            }

            memcpy(out + outLen, text, textLen);
            outLen += textLen;
        }
        else {

            // ordinary character

            if (outLen + 2 > cap) {
                cap *= 2;
                out = realloc(out, cap);
            }

            out[outLen++] = input[i];

            // newline handling
            if (input[i] == '\n') {
                atLineStart = 1;
                inDirective = 0;
            }
            else if (!isspace((unsigned char)input[i])) {
                atLineStart = 0;
            }

            i++;
        }
    }

    out[outLen] = '\0';

    return out;
}

void preprocess(struct ll_head* head, struct ll_head* includes, struct ll_head* macros) {
    for(size_t idx = 0; idx < head->len; idx++) {
        struct ll_node *srcLineNode_tmp = ll_get(head, idx);
        struct srcCodeLine *srcLine = srcLineNode_tmp->data;
        
        // handle macro expainsion
        {
            char* result = expandMacros(srcLine->str, macros);

            free(srcLine->str);
            srcLine->str = result;
            srcLine->str_len = strlen(result);
        }
        
        if(srcLine == NULL || srcLine->str_len <= 1 || srcLine->str[0] != '#') continue;

        if(srcLine->str_len > 9 && strncmp("#include ", srcLine->str, 9) == 0) {

            //    open file
            //    read each line an appendAfter $idx
            //    delete node $idx
            char* wsStripped = stripWhitespace(srcLine->str, srcLine->str_len);

            if((wsStripped[9] != '\"' || wsStripped[strlen(wsStripped) - 1] != '\"') && (wsStripped[9] != '<' || wsStripped[strlen(wsStripped) - 1] != '>')) {
                printf("Illegal Include. Name of included file must be put <> or \"\"\n");
            }
            char* includedFile_name = calloc(strlen(wsStripped) - 10, 1);
            strncpy(includedFile_name, &wsStripped[10], strlen(wsStripped) - 11);
                
            char *includedFile_path = NULL;
            if(wsStripped[9] == '\"') {
               if(wsStripped[10] != '/') includedFile_path = resolveRelPath(srcLine->file, false, includedFile_name);
                else includedFile_path = strdup(includedFile_name);
            } else {
                includedFile_path = resolveInIncludes(includedFile_name, includes);
            }
            free(wsStripped);

            FILE* includedFile_fd = fopen(includedFile_path, "r");
            if(includedFile_fd == NULL) {
                printf("Error opening file \"%s\"\n", resolveInIncludes(includedFile_name, includes));
                ll_remove(head, idx--);
                continue;
            }
            fseek(includedFile_fd, 0, SEEK_END);
            size_t includedFile_len = ftell(includedFile_fd);
            rewind(includedFile_fd);

            char* includedFile_buf = calloc(includedFile_len + 1, sizeof(char));
            fread(includedFile_buf, sizeof(char), includedFile_len, includedFile_fd);

            fclose(includedFile_fd);

            ll_remove(head, idx); 

            struct ll_head newFile_head = {
                .start = NULL,
                .end = NULL,
                .len = 0,
                .delete = srcCodeLine_delete,
            };

            {
                size_t lineNr = 1;
                size_t iF_idx = 0;
                size_t charCount = 0;
                while(includedFile_buf[iF_idx] != '\0') {
                    if(includedFile_buf[iF_idx] == '\n') {
                        char* line = calloc(charCount + 1, 1);
                        strncpy(line, &includedFile_buf[iF_idx - charCount], charCount);

                        struct srcCodeLine* srcLine = calloc(1, sizeof(struct srcCodeLine));
                        srcLine->str = line;
                        srcLine->str_len = charCount;
                        srcLine->file = strdup(includedFile_path);
                        srcLine->line = lineNr++;
                
                        ll_append(&newFile_head, srcLine);
                        charCount = 0;
                    } else charCount++;
                    iF_idx++;
                }
                if(charCount > 0) {
                    char* line = calloc(charCount + 1, 1);
                    strncpy(line, &includedFile_buf[iF_idx - charCount], charCount);

                    struct srcCodeLine* srcLine = calloc(1, sizeof(struct srcCodeLine));
                    srcLine->str = line;
                    srcLine->str_len = charCount;
                    srcLine->file = strdup(includedFile_path);
                    srcLine->line = lineNr++;
                    
                    ll_append(&newFile_head, srcLine);
                    charCount = 0;
                }
            }

            for(struct ll_node* cur = newFile_head.end; cur != NULL; cur = cur->prev) {
                struct srcCodeLine* srcLine = calloc(1, sizeof(struct srcCodeLine));
                struct srcCodeLine* curSrcLine = (struct srcCodeLine*)(cur->data);
                srcLine->str = strdup(curSrcLine->str);
                srcLine->str_len = curSrcLine->str_len;
                srcLine->file = strdup(curSrcLine->file); 
                srcLine->line = curSrcLine->line;
                ll_insertAs(head, idx, srcLine);
            }

            ll_delete(&newFile_head);

            free(includedFile_buf);

            free(includedFile_name);
            free(includedFile_path);

            idx--;

            continue;
        }
        
        if(srcLine->str_len > 8 && strncmp("#define ", srcLine->str, 8) == 0) {
            struct macro* currentMacro = calloc(1, sizeof(struct macro));

            char* macroNameStart = &srcLine->str[8];
            while(macroNameStart[0] == ' ' || macroNameStart[0] == '\t') {
                macroNameStart++;
            }

            size_t macroNameStart_len = 0;
            while(macroNameStart[macroNameStart_len] != ' '  &&
                  macroNameStart[macroNameStart_len] != '\t' &&
                  macroNameStart[macroNameStart_len] != '\0')   macroNameStart_len++;

            currentMacro->macroName = strndup(macroNameStart, macroNameStart_len);

            char* macroReplacementStart = &macroNameStart[macroNameStart_len];
            while(macroReplacementStart[0] == ' ' || macroReplacementStart[0] == '\t') {
                macroReplacementStart++;
            }

            size_t macroReplacementStart_len = 0;
            while(macroReplacementStart[macroReplacementStart_len] != ' '  &&
                  macroReplacementStart[macroReplacementStart_len] != '\t' &&
                  macroReplacementStart[macroReplacementStart_len] != '\0') macroReplacementStart_len++;

            currentMacro->replacement = strndup(macroReplacementStart, macroReplacementStart_len);

            if(currentMacro->macroName[0] >= '0' && currentMacro->macroName[0] <= '9') {
                printf(RED"%s:%lu Macro names MUST NOT begin with a numerical digit. Macro name \"%s\" violated one of these rules.\n"RESET, srcLine->file, srcLine->line, currentMacro->macroName);
                ll_remove(head, idx--);
                macros_delete(currentMacro);
                continue;
            }
            for(size_t i = 0; i < strlen(currentMacro->macroName); i++) {
                if( (currentMacro->macroName[i] < 'a' || currentMacro->macroName[i] > 'z') &&
                    (currentMacro->macroName[i] < 'A' || currentMacro->macroName[i] > 'Z') && 
                    (currentMacro->macroName[i] < '0' || currentMacro->macroName[i] > '9') &&
                    currentMacro->macroName[i] != '_') {
                    
                    printf(RED"%s:%lu Macros names MUST ONLY consist of a-z, A-Z, 0-9 and '_'. Macro name \"%s\" violated one of these rules.\n"RESET, srcLine->file, srcLine->line, currentMacro->macroName);
                    ll_remove(head, idx--);
                    continue;
                }
            }

            for(size_t i = 0; i < macros->len; i++) {
                struct macro* cur = ll_get(macros, i)->data;
                if(strcmp(cur->macroName, currentMacro->macroName) == 0) {
                    ll_remove(macros, i);
                    break;
                }
            }

            ll_append(macros, currentMacro);

            ll_remove(head, idx--);

            continue;
        }

        if(srcLine->str_len > 7 && strncmp("#undef ", srcLine->str, 7) == 0) {
            char* macroName = strdup(&srcLine->str[7]);
            for(size_t i = 0; i < macros->len; i++) {
                struct macro* cur = ll_get(macros, i)->data;
                if(strcmp(cur->macroName, macroName) == 0) {
                    ll_remove(macros, i);
                    
                    goto undef_end;
                }
            }

            printf(RED"%s:%lu Cannot undefine, macro \"%s\" does not exists\n"RESET, srcLine->file, srcLine->line, macroName);

            undef_end:

            ll_remove(head, idx--);

            free(macroName);
            continue;
        }

/*
 * Ifdef schenanigens
 * increment stack: ifdef ifndef
 * decrement stack: endif
 */

        if(srcLine->str_len > 7 && strncmp("#ifdef ", srcLine->str, 7) == 0) {
            char* macroName = strdup(&srcLine->str[7]);
            
            bool removeCode = true;

            for(size_t i = 0; i < macros->len; i++) {
                struct macro* cur = ll_get(macros, i)->data;
                if(strcmp(cur->macroName, macroName) == 0) {
                    removeCode = false;
                    break;
                }
            }

            size_t conditionalPreproStack = 1;
            ll_remove(head, idx);

            size_t retIdx = idx;

            while(conditionalPreproStack > 0 || idx < head->len) {
                struct srcCodeLine* cur = ll_get(head, idx)->data;

                if(strncmp(cur->str, "#ifdef ", 7) == 0 ||
                   strncmp(cur->str, "#ifndef ", 8) == 0) {
                    conditionalPreproStack++;
                }

                if(strncmp(cur->str, "#endif", 6) == 0) {
                    conditionalPreproStack--;
                    if(conditionalPreproStack == 0) {
                        ll_remove(head, idx);
                        break;
                    }
                }

                if(removeCode == true) ll_remove(head, idx);
                else idx++;
            }

            idx = --retIdx;

            free(macroName);
            continue;
        }

        if(srcLine->str_len > 7 && strncmp("#ifndef ", srcLine->str, 8) == 0) {
            char* macroName = strdup(&srcLine->str[8]);
            
            bool removeCode = false;

            for(size_t i = 0; i < macros->len; i++) {
                struct macro* cur = ll_get(macros, i)->data;
                if(strcmp(cur->macroName, macroName) == 0) {
                    removeCode = true;
                    break;
                }
            }

            size_t conditionalPreproStack = 1;
            ll_remove(head, idx);

            size_t retIdx = idx;

            while(conditionalPreproStack > 0 || idx < head->len) {
                struct srcCodeLine* cur = ll_get(head, idx)->data;

                if(strncmp(cur->str, "#ifdef ", 7) == 0 ||
                   strncmp(cur->str, "#ifndef ", 8) == 0) {
                    conditionalPreproStack++;
                }

                if(strncmp(cur->str, "#endif", 6) == 0) {
                    conditionalPreproStack--;
                    if(conditionalPreproStack == 0) {
                        ll_remove(head, idx);
                        break;
                    }
                }

                if(removeCode == true) ll_remove(head, idx);
                else idx++;
            }

            idx = --retIdx;

            free(macroName);
            continue;
        }
    }
}

void assemble(struct ll_head* head) {

    // assembler state info
    uint16_t offset = 0;
    struct ll_head labels = {
        .delete = &labels_delete,
        .len = 0,
        .start = NULL,
        .end = NULL,
    };

    struct ll_head statements = {
        .delete = &statement_delete,
        .len = 0,
        .start = NULL,
        .end = NULL,
    };

    for(struct ll_node *cur = head->start; cur != NULL; cur = cur->next) {
        struct srcCodeLine *src = cur->data;

        if(src->str_len == 0) continue;

        size_t commentCutoff = src->str_len;

        for(size_t i = 0; i < src->str_len - 1; i++) {
            if(src->str[i] == '/' && src->str[i + 1] == '/') {
                commentCutoff = i;
                break;
            }
        }

        char* instr = strndup(src->str, commentCutoff);
        

        bool nonEmpty = false;

        for(size_t i = 0; i < strlen(instr); i++) {
            if(instr[i] != ' ' && instr[i] != ',' && instr[i] != '\t') {
                nonEmpty = true;
                break;
            }
        }

        if(!nonEmpty) {
            free(instr);
            continue;
        }

        // 3 SCENARIOS:
        // 1. label
        // 2. assembler directives (@offset -esque things)
        // 3. actual instruction

        // rules for thee
        // 1. last non-whitespace is ':'
        // 2. first non-whitespace is '@'
        // 3. !1. && !2.

        struct ll_head tokens = {
            .delete = &tokenStr_delete,
            .len = 0,
            .start = NULL,
            .end = NULL,
        };

        char* tok = strtok(instr, " \t,");

        struct tok *token = calloc(1, sizeof(struct tok));
        token->tok = strdup(tok);
        token->src = strndup(src->str, commentCutoff);

        ll_append(&tokens, token);

        while(tok != NULL) {
            tok = strtok(NULL, " \t,");
            if(tok == NULL) break;
            
            struct tok *token = calloc(1, sizeof(struct tok));
            token->tok = strdup(tok);
            token->src = strndup(src->str, commentCutoff);
            ll_append(&tokens, token);
        }

        printf("Comparing \"%s\"\n", ((struct tok*)(ll_get(&tokens, 0))->data)->tok);

        if(((struct tok*)(ll_get(&tokens, 0))->data)->tok[0] == '@') {
            if(strcmp(((struct tok*)(ll_get(&tokens, 0))->data)->tok, "@offset") == 0) {
                char *end = 0;
                char *str = ((struct tok*)(ll_get(&tokens, 1)->data))->tok;
                long rawVal = strtol(str, &end, 0);

                if(str == end) {
                    printf("No number.\n");
                } else if(errno == ERANGE || (rawVal & 0xfff) != rawVal) {
                    printf("Number out of range.\n");
                } else if(*end != '\0') {
                    printf("Trailing junk: \"%s\"\n", end);
                } else {
                    struct statement *stmnt = calloc(1, sizeof(struct statement));
                    stmnt->type = STATEMENT_DIRECTIVE_OFFSET;
                    stmnt->data.offsetAddr = rawVal;

                    ll_append(&statements, stmnt);
                }
            } else if(strcmp(((struct tok*)(ll_get(&tokens, 0))->data)->tok, "@asciiz") == 0) {
                char* asciiz = ((struct tok*)(ll_get(&tokens, 1))->data)->src;

                asciiz += strlen("@asciiz");

                while(isspace((unsigned char)*asciiz)) asciiz++;

                asciiz = strdup(asciiz);
                char *oldAsciiz = asciiz;

                if(asciiz[0] != '"') {
                    printf("Did not find qouted string in @asciiz directive.\n");
                    free(oldAsciiz);
                    continue;
                }

                asciiz++;

                char *quotedBuf = calloc(strlen(asciiz) + 1, 1);

                size_t out = 0;

                bool borked = false;

                while(*asciiz && *asciiz != '"') {
                    char c = *asciiz++;

                    if(c == '\\') {
                        if(*asciiz == '\0') {
                            free(quotedBuf);
                            free(oldAsciiz);
                            printf("Invalid qouted string\n");
                            borked = true;
                            break;
                        }

                        c = *asciiz++;

                        switch(c) {
                            case 'n': c = '\n'; break;
                            case 'r': c = '\r'; break;
                            case 't': c = '\t'; break;
                            case '\\': c = '\\'; break;
                            case '"': c = '"'; break;

                            case 'x': {
                                int value = 0;
                                for(int i = 0; i < 2; i++) {
                                    char h = *asciiz++;

                                    if(!isxdigit((unsigned char)h)) {
                                        free(quotedBuf);
                                        free(oldAsciiz);
                                        printf("invalid hex code");
                                        borked = true;
                                        break;
                                    }

                                    value <<= 4;

                                    if(h >= '0' && h <= '9') value |= h - '0';
                                    else if(h >= 'a' && h <= 'f') value |= h - 'a' + 10;
                                    else value |= h - 'A' + 10;
                                }

                                c = (char)value;
                                break;
                            }

                            default: break;
                        }
                    }

                    quotedBuf[out++] = c;
                }

                if(borked) {
                    continue;
                }

                if(*asciiz != '"') {
                    free(quotedBuf);
                    free(oldAsciiz);
                    printf("Exit qoute missing\n");
                    continue;
                }

                struct statement *stmnt = calloc(sizeof(struct statement), 1);
                stmnt->type = STATEMENT_DIRECTIVE_ASCIIZ;
                stmnt->data.asciiz.buf = quotedBuf;
                stmnt->data.asciiz.size = out + 1;

                ll_append(&statements, stmnt);

                free(oldAsciiz);
            } else {
                // TODO: better error
                printf("Invalid directive\n");
            }
        } else if(((char*)(ll_get(&tokens, 0)->data))[strlen(((char*)(ll_get(&tokens, 0)->data))) - 1] == ':') {
            printf("Found label \"%s\"\n", (char*)(ll_get(&tokens, 0)->data));
            
            struct label *lab = calloc(1, sizeof(struct label));
            lab->addr = offset;
            lab->name = strdup((char*)(ll_get(&tokens, 0)->data));
            ll_append(&labels, lab);
            
            if(tokens.len > 1) {
                // Warning
                printf("Found Garbage after Label declaration.\n");
            }
        } else {
            // parse arm instruction
            // 1. check if its real
            // 2. check pattern
            // 3. parse args
            // 4. construct stmnt
        }

        ll_delete(&tokens);

        free(instr);
    }


    for(size_t i = 0; i < statements.len; i++) {
        struct statement *stmnt = ll_get(&statements, i)->data;

        switch(stmnt->type) {
            case STATEMENT_DIRECTIVE_OFFSET: {
                printf("Offset thingy: %i\n", stmnt->data.offsetAddr);
                break;
            }
            case STATEMENT_DIRECTIVE_ASCIIZ: {
                printf("ASCIIZ str: \"%s\"\n", stmnt->data.asciiz.buf);
                break;
            }
            case STATEMENT_CPUINSTRUCTION: {
                printf("Cpu instruction\n");
                break;
            }
        }
    }

    ll_delete(&statements);
}

void srcCodeLine_delete(void* data) {
    struct srcCodeLine* line = data;
    free(line->str);
    free(line->file);
    free(line);
}

void includes_delete(void* data) {
    char* include = data;
    free(include);
}

void macros_delete(void* data) {
    struct macro* macros = data;
    if(macros->macroName != NULL) free(macros->macroName);
    if(macros->replacement != NULL) free(macros->replacement);
    free(macros);
}

void tokenStr_delete(void* data) {
    struct tok* token = data;
    free(token->tok);
    free(token->src);
    free(token);
}

void labels_delete(void* data) {
    struct label *lab = data;
    free(lab->name);
    free(data);
}

void statement_delete(void* data) {
    struct statement* stmnt = data;

    if(stmnt->type == STATEMENT_DIRECTIVE_ASCIIZ) {
        free(stmnt->data.asciiz.buf);
    }

    if(stmnt->type == STATEMENT_CPUINSTRUCTION) {
        if(stmnt->data.instr.addr_label != NULL) free(stmnt->data.instr.addr_label);
    }

    free(stmnt);
}

int main(int __attribute__((unused)) argc, char __attribute__((unused)) **argv) {
    printf("Vectorcpu Raw ASsembler v0.0.0\n");
    
    struct ll_head includes_llhead = {
        .start = NULL,
        .end = NULL,
        .len = 0,
        .delete = includes_delete,
    };

    const char inputFile_name[] = "./asm/fib.asm";

    FILE* inputFile_fd = fopen(inputFile_name, "r");
    if(inputFile_fd == NULL) {
        printf("Failed to open input file \"%s\", exiting...\n", inputFile_name);
        return -1;
    }

    fseek(inputFile_fd, 0, SEEK_END);
    size_t inputFile_len = ftell(inputFile_fd);
    rewind(inputFile_fd);
    
    char* inputFile_buffer = calloc(inputFile_len + 1, 1);
    fread(inputFile_buffer, 1, inputFile_len, inputFile_fd);
    
    fclose(inputFile_fd);

    struct ll_head srcCode_llhead = {
        .start = NULL,
        .end = NULL,
        .len = 0,
        .delete = srcCodeLine_delete,
    };

    {
        size_t lineNr = 1;
        size_t iF_idx = 0;
        size_t charCount = 0;
        while(inputFile_buffer[iF_idx] != '\0') {
            if(inputFile_buffer[iF_idx] == '\n') {
                char* line = calloc(charCount + 1, 1);
                strncpy(line, &inputFile_buffer[iF_idx - charCount], charCount);

                struct srcCodeLine* srcLine = calloc(1, sizeof(struct srcCodeLine));
                srcLine->str = line;
                srcLine->str_len = charCount;
                srcLine->file = realpath(inputFile_name, NULL);
                srcLine->line = lineNr++;
                
                ll_append(&srcCode_llhead, srcLine);
                charCount = 0;
            } else charCount++;
            iF_idx++;
        }
        if(charCount > 0) {
            char* line = calloc(charCount + 1, 1);
            strncpy(line, &inputFile_buffer[iF_idx - charCount], charCount);

            struct srcCodeLine* srcLine = calloc(1, sizeof(struct srcCodeLine));
            srcLine->str = line;
            srcLine->str_len = charCount;
            srcLine->file = realpath(inputFile_name, NULL);
            srcLine->line = lineNr++;

            ll_append(&srcCode_llhead, srcLine);
        }
    }

    free(inputFile_buffer);

    struct ll_head macros_llhead = {
        .start = NULL,
        .end = NULL,
        .len = 0,
        .delete = macros_delete,
    };

    preprocess(&srcCode_llhead, &includes_llhead, &macros_llhead);

    assemble(&srcCode_llhead);

    /*
    printf("------------- CODE -------------\n");
    for(struct ll_node* cur = srcCode_llhead.start; cur != NULL; cur = cur->next) {
        printf("%s\n", ((struct srcCodeLine*)cur->data)->str);
    }

    printf("-------------MACROS-------------\n");
    for(struct ll_node* cur = macros_llhead.start; cur != NULL; cur = cur->next) {
        printf("\"%s\" -> \"%s\"\n", ((struct macro*)cur->data)->macroName, ((struct macro*)cur->data)->replacement);
    }
    */

    ll_delete(&srcCode_llhead);
    ll_delete(&includes_llhead);
    ll_delete(&macros_llhead);
}

/*

ABC
AE
A
AAAA
A

*/