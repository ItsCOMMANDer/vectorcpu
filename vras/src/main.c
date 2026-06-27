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

#define MKC_ADDR(x) (x & 0b111111111111)
#define MKC_PID(x) ((x & 0b111) << 2)
#define MKC_IMM8(x) (x & 0xff)
#define MKC_DST(x) ((x & 0b111) << 8)
#define MKC_SRC1(x) ((x & 0b111) << 5)
#define MKC_SRC2(x) ((x & 0b111) << 2)
#define MKC_SRC1ALT1(x) ((x && 0b111) << 8)
#define MKC_SRC1ALT2(x) ((x & 0b111) << 11)

#define MKI_PAT_A(opcode, addr) (opcode | MKC_ADDR(addr))
#define MKI_PAT_N(opcode) (opcode)
#define MKI_PAT_S(opcode, src) (opcode | MKC_SRC1(src))
#define MKI_PAT_D(opcode, dst) (opcode | MKC_DST(dst))
#define MKI_PAT_SP(opcode, src, pid) (opcode | MKC_SRC1(src))
#define MKI_PAT_DP(opcode, dst, pid) (opcode | MKC_DST(dst))
#define MKI_PAT_DS(opcode, dst, src) (opcode | MKC_SRC1(src) | MKC_DST(dst))
#define MKI_PAT_DSS(opcode, dst, src1, src2) (opcode | MKC_DST(dst) | MKC_SRC1(src1) | MKC_SRC2(src2))
#define MKI_PAT_DI(opcode, dst, imm8) (opcode | MKC_DST(dst) | MKC_IMM8(imm8))
#define MKI_PAT_SI(opcode, src, imm8) (opcode | MKC_SRC1ALT1(src) | MKC_IMM8(imm8))
#define MKI_PAT_SDI(opcode, src, dst, imm8) (opcode | MKC_DST(dst) | MKC_SRC1ALT2(src) | MKC_IMM8(imm8))

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

uint16_t parseAddr(char* tok, bool try) {
    char* end = NULL;
    errno = 0;
    long rawVal = strtol(tok, &end, 0);

    if(tok == end) {
        if(try) printf("No Number found in \"%s\".\n", tok);
        return 0xffff;
    } else if(errno == ERANGE || rawVal > 0xfff || rawVal < 0) {
        if(try) printf("Imm8 (\"%s\") out of range (must be 12 bit unsigned integer)\n", tok);
        return 0xffff;
    } else if(*end != '\0') {
        if(try) printf("Trailing junk (in \"%s\")\n", tok);
        return 0xffff;
    } else {
        return (uint16_t)rawVal;
    }
}

// parse label is literally just parseLable(char* tok) {return tok;}
char *regList[] = {
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
};

uint8_t parseReg(char* tok) {
    for(uint8_t i = 0; i < 8; i++) {
        if(strncmp(regList[i], tok, 3) == 0) return i;
    }
    printf("Invalid reg \"%s\"\n", tok);
    return 0xff;
}

uint8_t parsePort(char* tok) {
    char* end = NULL;
    errno = 0;
    long rawVal = strtol(tok, &end, 0);

    if(tok == end) {
        printf("No Number found in \"%s\".\n", tok);
        return 0xff;
    } else if(errno == ERANGE || rawVal > 7 || rawVal < 0) {
        printf("Imm8 (\"%s\") out of range (must be between 0 and 7)\n", tok);
        return 0xff;
    } else if(*end != '\0') {
        printf("Trailing junk (in \"%s\")\n", tok);
        return 0xff;
    } else {
        return (uint8_t)rawVal;
    }
}

uint16_t parseImm8(char* tok) {
    char* end = NULL;

    errno = 0;
    long rawVal = strtol(tok, &end, 0);

    if(tok == end) {
        printf("No Number found in \"%s\".\n", tok);
        return 0xffff;
    } else if(errno == ERANGE || rawVal > 255 || rawVal < -127) {
        printf("Imm8 (\"%s\") out of range (must be between -127 and 255)\n", tok);
        return 0xffff;
    } else if(*end != '\0') {
        printf("Trailing junk (in \"%s\")\n", tok);
        return 0xffff;
    } else {
        if(rawVal < 0) {
            return (uint16_t)(((uint8_t)(int8_t)rawVal) | 0x00 << 8);
        } else {
            return (uint16_t)(((uint8_t)rawVal) | 0x00 << 8);
        }
    }
}

struct instruction {
    enum ins_pattern pat;
    uint16_t opcode;
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

uint16_t resolveLabel(struct ll_head *labels, char* labelName) {
    for(size_t i = 0; i < labels->len; i++) {
        struct label* lab = ll_get(labels, i)->data;
        if(strcmp(lab->name, labelName) == 0) return lab->addr;
    }
    return 0xffff;
}

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0') 

uint16_t asmCpuInstr(struct instruction ins, struct ll_head *labels) {
    //printf("opcode used: %u\n", ins.opcode);
    switch(ins.pat) {
        case PAT_A: {
            if(ins.addr_label == NULL) return MKI_PAT_A(ins.opcode, ins.addr);
            else {
                uint16_t addr = resolveLabel(labels, ins.addr_label);
                if(addr == 0xffff) {
                    printf("Can't find label \"%s\", defaulting to 0\n", ins.addr_label);
                    return MKI_PAT_A(ins.opcode, 0);
                }
                return MKI_PAT_A(ins.opcode, addr);
            }
        }
        case PAT_N: return MKI_PAT_N(ins.opcode);
        case PAT_S: return MKI_PAT_S(ins.opcode, ins.src1);
        case PAT_D: {
            //printf("THINGY %u %u\n", ins.opcode, ins.dest);
            return MKI_PAT_D(ins.opcode, ins.dest);
        }
        case PAT_SP: return MKI_PAT_SP(ins.opcode, ins.src1, ins.pid);
        case PAT_DP: return MKI_PAT_DP(ins.opcode, ins.dest, ins.pid);
        case PAT_DS: return MKI_PAT_DS(ins.opcode, ins.dest, ins.src1);
        case PAT_DSS: return MKI_PAT_DSS(ins.opcode, ins.dest, ins.src1, ins.src2);
        case PAT_DI: return MKI_PAT_DI(ins.opcode, ins.dest, ins.imm8);
        case PAT_SI: return MKI_PAT_SI(ins.opcode, ins.src1, ins.imm8);
        case PAT_SDI: return MKI_PAT_SDI(ins.opcode, ins.src1, ins.dest, ins.imm8);
    }
    printf("Invalid instruction, defailting to halt");
    return 0b1001000000000111; // asm:halt
}

struct {
    char mnemonic[16];
    uint16_t opcode;
    enum ins_pattern pat;
} instructionFormats[] = {
    {.mnemonic = "jmp", .opcode = 0b0000000000000000, .pat = PAT_A},
    {.mnemonic = "call", .opcode = 0b0001000000000000, .pat = PAT_A},
    {.mnemonic = "bltu", .opcode = 0b0010000000000000, .pat = PAT_A},
    {.mnemonic = "bleu", .opcode = 0b0011000000000000, .pat = PAT_A},
    {.mnemonic = "blts", .opcode = 0b0100000000000000, .pat = PAT_A},
    {.mnemonic = "bles", .opcode = 0b0101000000000000, .pat = PAT_A},
    {.mnemonic = "beq", .opcode = 0b0110000000000000, .pat = PAT_A},
    {.mnemonic = "bne", .opcode = 0b0111000000000000, .pat = PAT_A},

    {.mnemonic = "nop",  .opcode = 0b1001000000000011, .pat = PAT_N},
    {.mnemonic = "halt", .opcode = 0b1001000000000111, .pat = PAT_N},
    {.mnemonic = "ret",  .opcode = 0b1001000000001011, .pat = PAT_N},
    {.mnemonic = "enzr", .opcode = 0b1001000000001111, .pat = PAT_N},
    {.mnemonic = "dszr", .opcode = 0b1001000000010011, .pat = PAT_N},
    
    {.mnemonic = "push", .opcode = 0b1001000000000000, .pat = PAT_S},
    {.mnemonic = "wrss", .opcode = 0b1001000000000100, .pat = PAT_S},
    {.mnemonic = "wrds", .opcode = 0b1001000000001000, .pat = PAT_S},
    {.mnemonic = "wrsp", .opcode = 0b1001000000001100, .pat = PAT_S},
    
    {.mnemonic = "pop",  .opcode = 0b1001000000000001, .pat = PAT_D},
    {.mnemonic = "rdss", .opcode = 0b1001000000000101, .pat = PAT_D},
    {.mnemonic = "rdds", .opcode = 0b1001000000001001, .pat = PAT_D},
    {.mnemonic = "rdsp", .opcode = 0b1001000000001101, .pat = PAT_D},
    
    {.mnemonic = "out", .opcode = 0b1001100000000000, .pat = PAT_SP},
    
    {.mnemonic = "in",  .opcode = 0b1001100000000001, .pat = PAT_DP},
    
    {.mnemonic = "mov",  .opcode = 0b1001000000000010, .pat = PAT_DS},
    {.mnemonic = "not",  .opcode = 0b1001000000000110, .pat = PAT_DS},
    {.mnemonic = "ldr",  .opcode = 0b1001000000001010, .pat = PAT_DS},
    {.mnemonic = "str",  .opcode = 0b1001000000001110, .pat = PAT_DS},
    
    {.mnemonic = "add",  .opcode = 0b1000000000000000, .pat = PAT_DSS},
    {.mnemonic = "sub",  .opcode = 0b1000000000000001, .pat = PAT_DSS},
    {.mnemonic = "adc",  .opcode = 0b1000000000000010, .pat = PAT_DSS},
    {.mnemonic = "sbc",  .opcode = 0b1000000000000011, .pat = PAT_DSS},
    {.mnemonic = "and",  .opcode = 0b1000100000000000, .pat = PAT_DSS},
    {.mnemonic = "or",   .opcode = 0b1000100000000001, .pat = PAT_DSS},
    {.mnemonic = "xor",  .opcode = 0b1000100000000010, .pat = PAT_DSS},

    {.mnemonic = "ldi",    .opcode = 0b1010000000000000, .pat = PAT_DI},
    {.mnemonic = "srldr",  .opcode = 0b1010100000000000, .pat = PAT_DI},
    
    {.mnemonic = "srstr",  .opcode = 0b1011000000000000, .pat = PAT_SI},
    
    {.mnemonic = "addi",  .opcode = 0b1100000000000000, .pat = PAT_SDI},
};

uint8_t* assemble(struct ll_head* head, size_t *bufSize) {

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

        //printf("Comparing \"%s\"\n", ((struct tok*)(ll_get(&tokens, 0))->data)->tok);

        if(((struct tok*)(ll_get(&tokens, 0))->data)->tok[0] == '@') {
            if(strcmp(((struct tok*)(ll_get(&tokens, 0))->data)->tok, "@offset") == 0) {
                char *end = NULL;
                char *str = ((struct tok*)(ll_get(&tokens, 1)->data))->tok;
                errno = 0;
                long rawVal = strtol(str, &end, 0);

                if(str == end) {
                    printf("No number.\n");
                } else if(errno == ERANGE || (rawVal & 0xfff) != rawVal) {
                    printf("Number out of range.\n");
                } else if(*end != '\0') {
                    printf("Trailing junk: \"%s\"\n", end);
                } else {
                    //struct statement *stmnt = calloc(1, sizeof(struct statement));
                    //stmnt->type = STATEMENT_DIRECTIVE_OFFSET;
                    //stmnt->data.offsetAddr = rawVal;

                    offset = rawVal;

                    //ll_append(&statements, stmnt);
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

                    ll_delete(&tokens);
                    free(instr);
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
                    ll_delete(&tokens);
                    free(instr);
                    continue;
                }

                if(*asciiz != '"') {
                    free(quotedBuf);
                    free(oldAsciiz);
                    printf("Exit qoute missing\n");

                    ll_delete(&tokens);
                    free(instr);
                    continue;
                }

                struct statement *stmnt = calloc(sizeof(struct statement), 1);
                stmnt->type = STATEMENT_DIRECTIVE_ASCIIZ;
                stmnt->data.asciiz.buf = quotedBuf;
                stmnt->data.asciiz.size = out + 1;

                offset += out + 1;

                ll_append(&statements, stmnt);

                free(oldAsciiz);
            } else {
                // TODO: better error
                printf("Invalid directive\n");
            }
        } else if(((struct tok*)(ll_get(&tokens, 0)->data))->tok[strlen(((struct tok*)(ll_get(&tokens, 0)->data))->tok) - 1] == ':') {
            //printf("Found label \"%s\"\n", (char*)(ll_get(&tokens, 0)->data));
            
            struct label *lab = calloc(1, sizeof(struct label));
            lab->addr = offset;
            lab->name = strndup(((struct tok*)(ll_get(&tokens, 0)->data))->tok, strlen(((struct tok*)(ll_get(&tokens, 0)->data))->tok) - 1);
            ll_append(&labels, lab);
            
            if(tokens.len > 1) {
                // Warning
                printf("Found Garbage after Label declaration.\n");
            }
        } else {
            // parse asm instruction
            // 1. check if its real
            // 2. check pattern
            // 3. parse args
            // 4. construct stmnt
            int instrID = -1;
            for(size_t i = 0; i < sizeof(instructionFormats) / sizeof(instructionFormats[0]); i++) {
                if(strncmp(((struct tok*)(ll_get(&tokens, 0)->data))->tok, instructionFormats[i].mnemonic, 15) == 0) {
                    //printf("INSTR IS \"%s\"\n", instructionFormats[i].mnemonic);
                    instrID = i;
                    break;
                }
            }

            if(instrID == -1) {
                printf("Unknown instruction \"%s\"\n", ((struct tok*)(ll_get(&tokens, 0)->data))->tok);
                
                ll_delete(&tokens);
                free(instr);
                continue;
            }

            struct statement *stmnt = calloc(sizeof(struct statement), 1);
            stmnt->type = STATEMENT_CPUINSTRUCTION;
            stmnt->data.instr.opcode = instructionFormats[instrID].opcode;

            switch(instructionFormats[instrID].pat) {
                case PAT_A: {
                    if(tokens.len > 2) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 2) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_A;

                    char* addrStr = ((struct tok*)(ll_get(&tokens, 1)->data))->tok;

                    
                    uint16_t addr = parseAddr(addrStr, false);
                    if(addr == 0xffff) {
                        bool borked = false;
                        for(size_t i = 0; i < strlen(addrStr); i++) {
                            if(!VALID_MACRO_NAME_CHAR(addrStr[i])) {
                                borked = true;
                                break;
                            }
                        }
                        if(borked) {
                            printf("Invalid addr\n");
                            free(stmnt);
                            goto err_ne_tok;
                        } else {
                            stmnt->data.instr.addr_label = strdup(addrStr);
                        }

                    } else stmnt->data.instr.addr = addr;
                    ll_append(&statements, stmnt);
                    break;
                }
                case PAT_N: {
                    if(tokens.len > 2) {
                        printf("This instr doesnt require args??\n");
                    } else if(tokens.len < 1) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }
                    stmnt->data.instr.pat = PAT_N;
                    stmnt->data.instr.opcode = instructionFormats[instrID].opcode;
                    ll_append(&statements, stmnt);
                    break;
                }
                case PAT_S: {
                    if(tokens.len > 2) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 2) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    uint8_t srcReg = parseReg(((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                    if(srcReg == 0xff) {
                        printf("Invalid register source identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_S;
                    stmnt->data.instr.src1 = srcReg;
                    ll_append(&statements, stmnt);
                    break;
                }
                case PAT_D: {
                    if(tokens.len > 2) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 2) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    uint8_t dstReg = parseReg(((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                    if(dstReg == 0xff) {
                        printf("Invalid register destination identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_D;
                    stmnt->data.instr.dest = dstReg;
                    ll_append(&statements, stmnt);
                    break;
                }
                case PAT_SP: {
                    if(tokens.len > 3) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 3) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    uint8_t srcReg = parseReg(((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                    uint8_t port = parsePort(((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                    if(srcReg == 0xff) {
                        printf("Invalid register source identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }
                    if(port == 0xff) {
                        printf("Invalid port \"%s\"\n", ((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_SP;
                    stmnt->data.instr.src1 = srcReg;
                    stmnt->data.instr.pid = port;

                    ll_append(&statements, stmnt);
                    break;
                }
                case PAT_DP: {
                    if(tokens.len > 3) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 3) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    uint8_t dstReg = parseReg(((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                    uint8_t port = parsePort(((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                    if(dstReg == 0xff) {
                        printf("Invalid register destination identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }
                    if(port == 0xff) {
                        printf("Invalid port \"%s\"\n", ((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_DP;
                    stmnt->data.instr.dest = dstReg;
                    stmnt->data.instr.pid = port;

                    ll_append(&statements, stmnt);
                    break;
                }
                case PAT_DS: {
                    if(tokens.len > 3) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 3) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    uint8_t dstReg = parseReg(((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                    uint8_t srcReg = parseReg(((struct tok*)(ll_get(&tokens, 2)->data))->tok);

                    if(dstReg == 0xff) {
                        printf("Invalid register destination identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    if(srcReg == 0xff) {
                        printf("Invalid register source identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_DS;
                    stmnt->data.instr.dest = dstReg;
                    stmnt->data.instr.src1 = srcReg;

                    ll_append(&statements, stmnt);
                    break;
                }
                case PAT_DSS: {
                    if(tokens.len > 4) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 4) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    uint8_t dstReg = parseReg(((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                    uint8_t src1Reg = parseReg(((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                    uint8_t src2Reg = parseReg(((struct tok*)(ll_get(&tokens, 3)->data))->tok);

                    if(dstReg == 0xff) {
                        printf("Invalid register destination identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    if(src1Reg == 0xff) {
                        printf("Invalid register source identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    if(src2Reg == 0xff) {
                        printf("Invalid register source identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 3)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_DSS;
                    stmnt->data.instr.dest = dstReg;
                    stmnt->data.instr.src1 = src1Reg;
                    stmnt->data.instr.src2 = src2Reg;

                    ll_append(&statements, stmnt);
                    break;
                }
                case PAT_DI: {
                    if(tokens.len > 3) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 3) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    uint8_t dstReg = parseReg(((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                    uint16_t imm8 = parseImm8(((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                    
                    if(dstReg == 0xff) {
                        printf("Invalid register destination identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    if((imm8 & 0xff00) != 0) {
                        printf("Invalid imm8 \"%s\"\n", ((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_DI;
                    stmnt->data.instr.dest = dstReg;
                    stmnt->data.instr.imm8 = imm8 & 0xff;
                    
                    ll_append(&statements, stmnt);
                    break;
                }
                case PAT_SI: {
                    if(tokens.len >= 2) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 2) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    uint8_t srcReg = parseReg(((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                    uint16_t imm8 = parseImm8(((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                    
                    if(srcReg == 0xff) {
                        printf("Invalid register source identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    if((imm8 & 0xff00) != 0) {
                        printf("Invalid imm8 \"%s\"\n", ((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_SI;
                    stmnt->data.instr.src1 = srcReg;
                    stmnt->data.instr.imm8 = imm8 & 0xff;
                    
                    ll_append(&statements, stmnt);
                    break;
                }

                case PAT_SDI: {
                    if(tokens.len > 4) {
                        printf("Too many tokens?\n");
                    } else if(tokens.len < 4) {
                        printf("Too little args for this (\"%s\").\n", ((struct tok*)ll_get(&tokens, 0)->data)->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    uint8_t dstReg = parseReg(((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                    uint8_t srcReg = parseReg(((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                    uint16_t imm8 = parseImm8(((struct tok*)(ll_get(&tokens, 3)->data))->tok);
                    
                    if(dstReg == 0xff) {
                        printf("Invalid register destination identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 1)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    if(srcReg == 0xff) {
                        printf("Invalid register source identifier \"%s\"\n", ((struct tok*)(ll_get(&tokens, 2)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    if((imm8 & 0xff00) != 0) {
                        printf("Invalid imm8 \"%s\"\n", ((struct tok*)(ll_get(&tokens, 3)->data))->tok);
                        free(stmnt);
                        goto err_ne_tok;
                    }

                    stmnt->data.instr.pat = PAT_SDI;
                    stmnt->data.instr.dest = dstReg;
                    stmnt->data.instr.src1 = srcReg;
                    stmnt->data.instr.imm8 = imm8 & 0xff;
                    
                    ll_append(&statements, stmnt);
                    break;
                }
            }
            offset += 2;
        }
        
err_ne_tok:
        ll_delete(&tokens);
        free(instr);
    }

    
    //printf("------------------STATEMENTS------------------------\n");

    size_t totalSize = 0;

    for(size_t i = 0; i < statements.len; i++) {
        struct statement *stmnt = ll_get(&statements, i)->data;

        switch(stmnt->type) {
            case STATEMENT_DIRECTIVE_OFFSET: {
                //printf("Offset thingy: %i\n", stmnt->data.offsetAddr);
                break;
            }
            case STATEMENT_DIRECTIVE_ASCIIZ: {
                //printf("ASCIIZ str: \"%s\"\n", stmnt->data.asciiz.buf);
                totalSize += stmnt->data.asciiz.size;
                break;
            }
            case STATEMENT_CPUINSTRUCTION: {
                //printf("Cpu instruction\n");
                totalSize += 2;
                break;
            }
        }
    }

    /*
    printf("------------------LABELS------------------------\n");
    for(size_t i = 0; i < labels.len; i++) {
        struct label *lab = ll_get(&labels, i)->data;
        printf("\"%s\" -> %i\n", lab->name, lab->addr);
    }
    */

    uint8_t* buf = calloc(totalSize, 1);
    size_t idx = 0;

    for(size_t i = 0; i < statements.len; i++) {
        struct statement *stmnt = ll_get(&statements, i)->data;

        switch(stmnt->type) {
            case STATEMENT_DIRECTIVE_ASCIIZ: {
                memcpy(&buf[idx], stmnt->data.asciiz.buf, stmnt->data.asciiz.size);
                idx += stmnt->data.asciiz.size;
                break;
            }
            case STATEMENT_CPUINSTRUCTION: {
                uint16_t instr = asmCpuInstr(stmnt->data.instr, &labels);
                memcpy(&buf[idx], &instr, 2);
                idx += 2;
                break;
            }
            case STATEMENT_DIRECTIVE_OFFSET: break;
        }
    }

    if(idx != totalSize) printf("SOMEHTING is wrong, calced tot size = %lu and idx at end is = %lu\n", totalSize, idx);

    ll_delete(&labels);
    ll_delete(&statements);

    *bufSize = totalSize;
    return buf;
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

/*
 * CLI Args:
 *  -i <input_file>
 *  -o <output_file>
 *  -I <include_dir>
 *  -h help
 */

int main(int argc, char **argv) {
    printf("Vectorcpu Raw ASsembler v0.0.0\n");
    
    char* inputFile_name = NULL;
    char* outputFile_name = NULL;

    struct ll_head includes_llhead = {
        .start = NULL,
        .end = NULL,
        .len = 0,
        .delete = includes_delete,
    };

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-h] -i <input file> -o <output file> [-I <include directory> ...]\n", argv[0]);
            printf("\t-h : display this help message.\n");
            printf("\t-i : filename for the input assembly file\n");
            printf("\t-o : filename for the assembled binary blob\n");
            printf("\t-I : directory name for the include path\n");
            return 0;
        } else if(strcmp(argv[i], "-i") == 0) {
            if(inputFile_name != NULL) {
                printf("Multiple input files given, only one can be accepted.\n");
                free(inputFile_name);
                if(!outputFile_name) free(outputFile_name);
                return 1;
            }
            inputFile_name = strdup(argv[++i]);
        } else if(strcmp(argv[i], "-o") == 0) {
            if(outputFile_name != NULL) {
                printf("Multiple output files given, only one can be accepted.\n");
                free(outputFile_name);
                if(!inputFile_name) free(inputFile_name);
                return 1;
            }
            outputFile_name = strdup(argv[++i]);
        } else if(strcmp(argv[i], "-I") == 0) {
            if(argc - i == 0) {
                printf("No direcotry given for include, skipping");
            } else {
                ll_append(&includes_llhead, strdup(argv[++i]));
            }
        }
    }

    if(inputFile_name == NULL) {
        printf("No input file given.\n");
        return 1;
    }

    if(outputFile_name == NULL) {
        printf("No output file given.\n");
        return 1;
    }

    //const char inputFile_name[] = "./asm/fib.asm";

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

    size_t binBufSize = 0;

    uint8_t* binBuf = assemble(&srcCode_llhead, &binBufSize);

    FILE* outputFile_fd = fopen(outputFile_name, "wb");

    fwrite(binBuf, 1, binBufSize, outputFile_fd);

    fclose(outputFile_fd);

    free(binBuf);

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

    free(inputFile_name);
    free(outputFile_name);
}

/*

ABC
AE
A
AAAA
A

*/