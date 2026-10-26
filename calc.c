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
// commit messages where mostly written by VS Code GitHub Copilot
// further usage down below

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define STUDENT_NAME "bengt"
#define STUDENT_LASTNAME "bucker"
#define STUDENT_ID "260ADM047"

//---- File I/O ----
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

//---- Tokenizer ----
enum TokenType {
    TOKEN_NUMBER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULT,
    TOKEN_DIV,
    TOKEN_PARENTHESES_OPEN,
    TOKEN_PARENTHESES_CLOSE,
    TOKEN_POWER,
    TOKEN_EOL,
    TOKEN_EOF,
    TOKEN_INVALID,
};

struct Token {
    enum TokenType type;
    long long value;
    size_t pos;
};

//Struct instead of global vars so the data flow stays explicit and easier to manage
struct Tokenizer {
    const char *buffer;
    size_t length;
    size_t position;
    int has_error;
    size_t error_position;
    struct Token peek;
    int has_peeked;
};

//Position = buffer index + 1 (1-based position for error reporting)
static struct Token next_token(struct Tokenizer *t) {
    
    if (t->has_peeked) {
        t->has_peeked = 0;
        return t->peek;
    }

    //remove whitespace
    while (t->buffer[t->position] == ' ' || t->buffer[t->position] == '\t') {
        t->position++;
    }

    if (t->position >= t->length) {
        struct Token token = {TOKEN_EOF, 0, t->length + 1};
        return token;
    }
    
    // One rule for every branch
    char current = t->buffer[t->position];
    switch (current) {
        case '\n': {
            struct Token token = {TOKEN_EOL, 0, t->position + 1};
            t->position++;
            return token;
        }
        case '+': {
            struct Token token = {TOKEN_PLUS, 0, t->position + 1};
            t->position++;
            return token;
        }
        case '-': {
            struct Token token = {TOKEN_MINUS, 0, t->position + 1};
            t->position++;
            return token;
        }
        case '*': {
            //check for "**"
            //because of -1, the next character is always readable
            if (t->buffer[t->position + 1] == '*') {
                struct Token token = {TOKEN_POWER, 0, t->position + 1};
                t->position += 2;
                return token;
            }
            struct Token token = {TOKEN_MULT, 0, t->position + 1};
            t->position++;
            return token;
        }
        case '/': {
            struct Token token = {TOKEN_DIV, 0, t->position + 1};
            t->position++;
            return token;
        }
        case '(': {
            struct Token token = {TOKEN_PARENTHESES_OPEN, 0, t->position + 1};
            t->position++;
            return token;
        }
        case ')': {
            struct Token token = {TOKEN_PARENTHESES_CLOSE, 0, t->position + 1};
            t->position++;
            return token;
        }
        case '0' ... '9': {
            size_t start = t->position;
            long long value = 0;
            while (t->buffer[t->position] >= '0' && t->buffer[t->position] <= '9') {
                value = value * 10 + (t->buffer[t->position] - '0');
                t->position++;
            }
            return (struct Token){TOKEN_NUMBER, value, start + 1};
        }
        default: {
            struct Token token = {TOKEN_INVALID, 0, t->position + 1};
            t->position++;
            return token;
        }
    }
}

//look at the next token without consuming it
static struct Token peek_token(struct Tokenizer *t) {
    if (t->has_peeked) {
        return t->peek;
    }
    t->peek = next_token(t);
    t->has_peeked = 1;
    return t->peek;
}

//---- Parser ----
//needed for term() to call power()
static long long power(struct Tokenizer *t);
//needed for primary() to call expression() recursively when encountering parentheses
static long long expression(struct Tokenizer *t, enum TokenType end_token);
//ERROR postion is the tokens position (1-based) where the first error occurred
static long long primary(struct Tokenizer *t) {
    struct Token token = next_token(t);
   
    // ( opens a group -> is done in the same recursion -> nesting of any depth works
    if (token.type == TOKEN_NUMBER) {
        return token.value;
    } else if (token.type == TOKEN_PARENTHESES_OPEN) {
        long long value = expression(t, TOKEN_PARENTHESES_CLOSE);
        if (t->has_error) {
            return 0;
        }
        return value;
    } else {
        if (t->has_error == 0) { //first ERROR wins
            t->has_error = 1;
            t->error_position = token.pos;
        }
        return 0;
    }
}

//handles mult and div
//runs inside expression() so 3 + 4 * 5 becomes 3 + (4 * 5)
static long long term(struct Tokenizer *t) {
    long long solution = power(t);

    if (t->has_error) {
        return 0;
    }

    while (peek_token(t).type == TOKEN_MULT || peek_token(t).type == TOKEN_DIV) {
        struct Token token = next_token(t);
        if (token.type == TOKEN_MULT) {
            solution *= power(t);
            if (t->has_error) {
                return 0;
            }
        } else if (token.type == TOKEN_DIV) {
            //divison by zeor is reported at the start of the right operand
            long long div_with_zero_pos = peek_token(t).pos;
            long long div_with_zero = power(t);

            if (t->has_error) {
                return 0;
            }
            if (div_with_zero == 0) {
                t->has_error = 1;
                t->error_position = div_with_zero_pos;
            } else {
                solution /= div_with_zero;
            }
        }
    }

    return solution;
}

//handles power
//rightassociative a ** b ** c is parsed as a ** (b ** c)
static long long power(struct Tokenizer *t) {
    long long value = primary(t);
    if (t->has_error) {
        return 0;
    }

    if (peek_token(t).type == TOKEN_POWER) {
        next_token(t);
        long long pos = peek_token(t).pos;
        long long exponent = power(t);

        if (t->has_error) {
            return 0;
        }

        if (exponent < 0) {
            t->has_error = 1;
            t->error_position = pos;
            return 0;
        } else {
            long long result = 1;
            for (long long i = 0; i < exponent; i++) {
                result *= value;
            }
            value = result;
        }
    }

    return value;
}

//handles addition and subtraction
//leftassociative 
static long long expression(struct Tokenizer *t, enum TokenType end_token) {
    long long solution = term(t);

    if (t->has_error) {
        return 0;
    }

    while (peek_token(t).type == TOKEN_PLUS || peek_token(t).type == TOKEN_MINUS) {
        struct Token token = next_token(t);
        if (token.type == TOKEN_PLUS) {
            solution += term(t);
            if (t->has_error) {
                return 0;
            }
        } else if (token.type == TOKEN_MINUS) {
            solution -= term(t);
            if (t->has_error) {
                return 0;
            }
        } 
    }

    //States which token is allowed to end the expression
    //- for a "normal" line: TOKEN_EOL
    //- for a group: TOKEN_PARENTHSES_CLOSE
    // - for the end of file: TOKEN_EOF
    struct Token ende = peek_token(t);
    
    if (ende.type == end_token || (end_token == TOKEN_EOL && ende.type == TOKEN_EOF)) {
        next_token(t);
        return solution;
    } else {
        if (t->has_error == 0) {
            t->has_error = 1;
            t->error_position = ende.pos;
        }
        return 0;
    }
}

//---- Main ----
//results are collected before writing to the output
//file to ensure that if any line has an error
//only the error is written and no partial results are output
// I used AI (VS Code GitHub Copilot and OpenCode GLM-5.3-Flash) to fit the main method to the current state of the file (the different commits), sessions are not shareable
//- e.g. change var or method names
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

    FILE *out = fopen(output_path, "w");
    if (out == NULL) {
        fprintf(stderr, "Failed to open '%s' for writing\n", output_path);
        free(buffer);
        free(output_path);
        return 1;
    }

    struct Tokenizer t = {0};
    t.buffer = buffer;
    t.length = length;

    size_t capacity = 32;
    size_t count = 0;
    long long *results = malloc(capacity * sizeof *results);
    if (results == NULL) {
        fprintf(stderr, "Out of memory\n");
        fclose(out);
        free(buffer);
        free(output_path);
        return 1;
    }


    while (t.position < t.length) {
        //find EOL
        size_t line_end = t.position;
        while (line_end < t.length && t.buffer[line_end] != '\n') {
            line_end++;
        }

        //find first none whitespace character
        size_t b = t.position;
        while (b < t.length && (t.buffer[b] == ' ' || t.buffer[b] == '\t')) {
            b++;
        }

        //skip empty lines
        if (b < line_end && buffer[b] == '#') {
            t.position = line_end + 1;
            continue;
        }
        if (b == line_end) {
            t.position = line_end + 1;
            continue;
        }

        long long r = expression(&t, TOKEN_EOL);
        if (t.has_error) {
            break;
        }
        if (count == capacity) {
            capacity *= 2;
            long long *grown = realloc(results, capacity * sizeof *results);
            if (grown == NULL) {
                fprintf(stderr, "Out of memory\n");
                free(results);
                fclose(out);
                free(buffer);
                free(output_path);
                return 1;
            }
            results = grown;
        }
        results[count++] = r;
    }

    if (t.has_error) {
        fprintf(out, "ERROR:%zu\n", t.error_position);
    } else {
        for (size_t i = 0; i < count; i++) {
            fprintf(out, "%lld\n", results[i]);
        }
    }

    fclose(out);
    free(results);

    free(buffer);
    free(output_path);
    return 0;
}
