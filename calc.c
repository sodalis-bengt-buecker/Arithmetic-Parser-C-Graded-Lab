// Bengt Bücker 260ADM047
// https://github.com/sodalis-bengt-buecker/Arithmetic-Parser-C-Graded-Lab
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
//Resources used:
//https://deepwiki.com/codgician/parsic/5-building-a-calculator-parser
//https://codemia.io/knowledge-hub/path/techniques_needed_to_write_an_arithmetic_expression_parser
//AI: (Mistral Medium 3.5) help with Project Planning: https://chat.mistral.ai/work/eb453a14-e422-41b7-b30d-9f30d040b6b6

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define STUDENT_NAME "bengt"
#define STUDENT_LASTNAME "bucker"
#define STUDENT_ID "260ADM047"

int main(void) {
    printf("hello\n");
    return 0;
}

// Reads the entire input file into one heap buffer.
static char *read_whole_file(const char *path, size_t *length_out) {

    FILE *file = fopen(path, "rb");
    if (file == NULL){
        return NULL;
    }

    // Start with a small buffer and grow as needed. (So files of any size can be read)
    size_t max_capacity = 32;
    size_t used_capacity = 0;
    char *buffer = malloc(max_capacity);

    // Check for errors while allocating the buffer
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    while (true) {
        if (used_capacity == max_capacity - 1) {
            max_capacity *= 2;
            char *new_buffer = realloc(buffer, max_capacity);
            if (new_buffer == NULL) {
                free(buffer);
                fclose(file);
                return NULL;
            }
            buffer = new_buffer;
        }
        
        size_t bytes_read = fread(buffer + used_capacity, 1, max_capacity - used_capacity - 1, file);
        if (bytes_read == 0) {
            break;
        }
        used_capacity += bytes_read;
    }

    buffer[used_capacity] = '\0';
    if (length_out != NULL) {
        *length_out = used_capacity;
    }
    fclose(file);
    return buffer;



}
