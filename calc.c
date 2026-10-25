// Bengt Bücker 260ADM047
// https://github.com/sodalis-bengt-buecker/Arithmetic-Parser-C-Graded-Lab
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
//Resources used:
//https://deepwiki.com/codgician/parsic/5-building-a-calculator-parser
//https://codemia.io/knowledge-hub/path/techniques_needed_to_write_an_arithmetic_expression_parser
//AI: 
// VSCode GitHub Copilot auto completer
// Mistral Medium 3.5 help with Project Planning: https://chat.mistral.ai/work/eb453a14-e422-41b7-b30d-9f30d040b6b6
// GLM-5.3-Flash created the UML (The Chat is not shareable)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define STUDENT_NAME "bengt"
#define STUDENT_LASTNAME "bucker"
#define STUDENT_ID "260ADM047"


// Reads the entire input file into one heap buffer
static char *read_whole_file(const char *path, size_t *length_out) {

    FILE *file = fopen(path, "rb");
    if (file == NULL){
        return NULL;
    }

    // Start with a small buffer and grow as needed (So files of any size can be read)
    size_t max_capacity = 32;
    size_t used_capacity = 0;
    char *buffer = malloc(max_capacity);

    // Check for errors while allocating the buffer
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    for (;;) {
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
        
        // -1 reserves the last byte for the '\0'
        size_t bytes_read = fread(buffer + used_capacity, 1, max_capacity - used_capacity - 1, file);
        if (bytes_read == 0) {
            break;
        }
        used_capacity += bytes_read;
    }

    buffer[used_capacity] = '\0';
    *length_out = used_capacity;
    fclose(file);
    return buffer;

}

// Build "OUTPUT_DIRECTORY/<base>_<name>_<lastname>_<id>.txt" for an input path.
static char *create_output_path(const char *output_path, const char *input_path) {
    const char *slash = strrchr(input_path, '/');
    const char *base = (slash != NULL) ? slash + 1 : input_path;
    const char *dot = strrchr(base, '.');

    size_t base_len = strlen(base);

    if (dot != NULL && strcmp(dot, ".txt") == 0) {
        base_len = (size_t)(dot - base);
    }

    // snprintf never writes more than len bytes so the size must fit the longest possible result exactly
    size_t len = strlen(output_path) + 1
               + base_len + 1
               + strlen(STUDENT_NAME) + 1
               + strlen(STUDENT_LASTNAME) + 1
               + strlen(STUDENT_ID) + 4
               + 1;
    
    char *path = malloc(len);
    if (path == NULL) {
        return NULL;
    }

    // *s leaves out txt
    snprintf(path, len, "%s/%.*s_%s_%s_%s.txt",
             output_path,
             (int)base_len, base,
             STUDENT_NAME,
             STUDENT_LASTNAME,
             STUDENT_ID);

    return path;
}

// Creates the output directory if it is missing
static int ensure_output_directory(const char *output_dir) {
    if (mkdir(output_dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Failed to create output directory '%s'\n", output_dir);
        return 1;
    }
    return 0;
}

// Writes the whole buffer to the file. Returns 0 on success, nonzero on error.
static int write_file(const char *path, const char *content, size_t length) {
    FILE *file = fopen(path, "w");
    
    if (file == NULL) {
        fprintf(stderr, "Failed to open file '%s' for writing\n", path);
        return 1;
    }

    fwrite(content, 1, length, file);
    fclose(file);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[1], "-o") != 0) {
        fprintf(stderr, "Usage: %s -o <output_dir> <input_file>\n", argv[0]);
        return 1;
    }

    size_t length;
    char *buffer = read_whole_file(argv[3], &length);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to read input file '%s'\n", argv[3]);
        return 1;
    }

    if (ensure_output_directory(argv[2]) != 0) {
        free(buffer);
        return 1;
    }
    
    char *output_path = create_output_path(argv[2], argv[3]);
    if (output_path == NULL) {
        free(buffer);
        return 1;
    }

    int yippie = write_file(output_path, buffer, length);

    free(buffer);
    free(output_path);
    return yippie;
}
