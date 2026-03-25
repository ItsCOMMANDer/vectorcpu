#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <libgen.h>
#include <assert.h>
#include <unistd.h>

#include "linkedList.h"


struct srcCodeLine {
    char* str;
    size_t str_len;
    size_t line;
    char* file;
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
    }
    return NULL;
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

void preprocess(struct ll_head* head, struct ll_head* includes) {
    for(size_t idx = 0; idx < head->len; idx++) {
        struct ll_node *srcLineNode_tmp = ll_get(head, idx);
        struct srcCodeLine *srcLine = srcLineNode_tmp->data;
        if(srcLine != NULL && srcLine->str_len > 1 && srcLine->str[0] == '#') {
            if(srcLine->str_len > 9 && strncmp("#include ", srcLine->str, 9) == 0) {
                //    open file
                //    read each line an appendAfter $idx
                //    delete node $idx
                char* wsStripped = calloc(srcLine->str_len + 1, 1);
                size_t wssIdx = 0;

                bool inQuote = false;

                for(size_t i = 0; i < srcLine->str_len; i++) {
                    if((srcLine->str[i] != ' ' && srcLine->str[i] != '\t') || inQuote) {
                        wsStripped[++wssIdx - 1] = srcLine->str[i];
                        if(srcLine->str[i] == '\"') inQuote = !inQuote;
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
                if((wsStripped[9] != '\"' || wsStripped[wssIdx - 1] != '\"') && (wsStripped[9] != '<' || wsStripped[wssIdx - 1] != '>')) {
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
                    printf("Error opening file\n");
                }
                fseek(includedFile_fd, 0, SEEK_END);
                size_t includedFile_len = ftell(includedFile_fd);
                rewind(includedFile_fd);

                char* includedFile_buf = calloc(includedFile_len + 1, sizeof(char));
                fread(includedFile_buf, sizeof(char), includedFile_len, includedFile_fd);

                fclose(includedFile_fd);

                ll_remove(head, idx);

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
                
                            ll_insertBefore(head, idx++, srcLine);
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
                    
                        ll_insertBefore(head, idx++, srcLine);
                        charCount = 0;
                    }
                }

                idx--;

                free(includedFile_buf);

                free(includedFile_name);
                free(includedFile_path);

            }
        }
    }
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

int main(int __attribute__((unused)) argc, char __attribute__((unused)) **argv) {
    printf("Vectorcpu Raw ASsembler v0.0.0\n");
    
    struct ll_head includes_llhead = {
        .start = NULL,
        .end = NULL,
        .len = 0,
        .delete = includes_delete,
    };

    char *incl_tmp = strdup("/home/niklas/Desktop/projects/C/vectorcpu/vras/asm/include");
    ll_append(&includes_llhead, incl_tmp);

    const char inputFile_name[] = "./asm/test.as";

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

    preprocess(&srcCode_llhead, &includes_llhead);

    for(struct ll_node* cur = srcCode_llhead.start; cur != NULL; cur = cur->next) {
        printf("%s\n", ((struct srcCodeLine*)cur->data)->str);
    }

    ll_delete(&srcCode_llhead);
    ll_delete(&includes_llhead);
}

/*

ABC
AE
A
AAAA
A

*/