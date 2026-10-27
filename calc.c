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
// Opus-5.5 in the end I ran a prompt "Checke mein fertiges [calc.c](calc.c) gegen die Anforderungen (einene Ordner darüber). Erstelle hier im chat eine übersicht, was ich noch fixen muss. Fasse das Dokument selber nicht an." to check my calc.c against the course requirements, the changes resulting by that are in the last commit and depicted wiht "AI Refactor" in the code
// GLM-5.3-Flash was used to enhance my comments, and fix my english
//further usage down below

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>

#define STUDENT_NAME "bengt"
#define STUDENT_LASTNAME "bucker"
#define STUDENT_ID "260ADM047"

//---- Types ----
enum TokenType {
    TOKEN_NUMBER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULT,
    TOKEN_DIV,
    TOKEN_PARENTHESES_OPEN,
    TOKEN_PARENTHESES_CLOSE,
    TOKEN_POWER,
    TOKEN_FLOAT,
    TOKEN_EOL,
    TOKEN_EOF,
    TOKEN_INVALID,
};

struct Token {
    enum TokenType type;
    long long value;
    size_t pos;
    double float_value;
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

struct Value {
    int is_float;
    long long int_value;
    double float_value;
};

//--- Constructors for Value struct ---
static struct Value make_integer(long long value) {
    struct Value v;
    v.is_float = 0;
    v.int_value = value;
    v.float_value = 0.0;
    return v;
}

static struct Value make_float(double value) {
    struct Value v;
    v.is_float = 1;
    v.int_value = 0;
    v.float_value = value;
    return v;
}

//whole calc runs as double
//if both int -> stays int
static double as_double(struct Value v) {
    if (v.is_float) {
        return v.float_value;
    } else {
        return (double)v.int_value;
    }
}

//---- Prototypes ----
//Declarations for functions that are used before they are defined further down
static const char *file_base_name(const char *path, size_t *length_out);
//needed for term() to call power()
static struct Value power(struct Tokenizer *t);
//needed for primary() to call expression() recursively when encountering parentheses
static struct Value expression(struct Tokenizer *t, enum TokenType end_token);

//---- File I/O ----
//used pseudo code by GLM-5.3-Flash
// Reads the entire input file into one heap buffer
static char *read_whole_file(const char *path, size_t *length_out) {

    FILE *file = fopen(path, "rb");
    if (file == NULL){
        return NULL;
    }

    // AI Refactor:
    // Large files: the file is read only once into one buffer. When the buffer is
    // full, its size is doubled, so even a file of many MB needs only a few reallocs.
    // Later the parser walks this buffer with an index and never copies lines.

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

    size_t base_len;
    const char *file_base = file_base_name(input_path, &base_len);

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
             (int)base_len, file_base,
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

//removed from main
// processes a single input file and writes the results to the output path.
static int process_file(const char *input_path, const char *out_path) {
    size_t length;
    char *buffer = read_whole_file(input_path, &length);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to read input file '%s'\n", input_path);
        return 1;
    }

    if (ensure_output_directory(out_path) != 0) {
        free(buffer);
        return 1;
    }
    
    char *output_path = create_output_path(out_path, input_path);
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
    struct Value *results = malloc(capacity * sizeof *results);

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

        struct Value r = expression(&t, TOKEN_EOL);
        if (t.has_error) {
            break;
        }
        if (count == capacity) {
            capacity *= 2;
            struct Value *grown = realloc(results, capacity * sizeof *results);
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
            
            //15 digits, trailing zeros stripped
            if (results[i].is_float) {
                fprintf(out, "%.15g\n", results[i].float_value);
            } else {
                fprintf(out, "%lld\n", results[i].int_value);
            }

        }
    }

    // AI Refactor: fclose writes the rest of the stdio buffer, so a full disk only shows up here
    if (fclose(out) != 0) {
        fprintf(stderr, "Failed to write '%s'\n", output_path);
        free(results);
        free(buffer);
        free(output_path);
        return 1;
    }
    free(results);

    free(buffer);
    free(output_path);
    return 0;
}

//used pseudo code by GLM-5.3-Flash
//processes every .txt file in the given directory -> results in own output_dir
static int process_directory(const char *dir_path, const char *output_dir) {
    DIR *folder = opendir(dir_path);
    if (folder == NULL) {
        fprintf(stderr, "Failed to open directory: %s\n", dir_path);
        return 1;
    }
    int ergebnis = 0;
    struct dirent *entry;

    // Iterate over all entries in the directory
    //
    while ((entry = readdir(folder)) != NULL) {
        const char *name = entry->d_name;
        size_t name_len = strlen(name);


        if (name_len < 4 || strcmp(name + name_len - 4, ".txt") != 0) {
            continue;
        }

        char full_path[strlen(dir_path) + 1 + strlen(name) + 1];
        //
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, name);
        struct stat st;

        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }
        if (process_file(full_path, output_dir) != 0) {
            ergebnis = 1;
        }
    }
    closedir(folder);
    return ergebnis;
}

static const char *file_base_name(const char *path, size_t *length_out) {
    const char *slash = strrchr(path, '/');
    const char *base = (slash != NULL) ? slash + 1 : path;
    const char *dot = strrchr(base, '.');
    size_t length = strlen(base);

    if (dot != NULL && strcmp(dot, ".txt") == 0) {
        length = (size_t)(dot - base);
    }
    if (length_out != NULL) {
        *length_out = length;
    }
    return base;
}

//---- Tokenizer ----
// The whole file is in one buffer and t->position is the index into it. The index is
// never reset per line, so every '\n' and every character of comment lines is counted
// automatically. A token stores the index of its first character + 1, because
// positions are 1-based. The end of the file gets length + 1: that is the position
// for a missing operand or ')' when the last line has no '\n'.
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
        struct Token token = {TOKEN_EOF, 0, t->length + 1, 0.0};
        return token;
    }
    
    // One rule for every branch
    char current = t->buffer[t->position];
    switch (current) {
        case '\n': {
            struct Token token = {TOKEN_EOL, 0, t->position + 1, 0.0};
            t->position++;
            return token;
        }
        case '+': {
            struct Token token = {TOKEN_PLUS, 0, t->position + 1, 0.0};
            t->position++;
            return token;
        }
        case '-': {
            struct Token token = {TOKEN_MINUS, 0, t->position + 1, 0.0};
            t->position++;
            return token;
        }
        case '*': {
            //check for "**"
            //because of -1, the next character is always readable
            if (t->buffer[t->position + 1] == '*') {
                struct Token token = {TOKEN_POWER, 0, t->position + 1, 0.0};
                t->position += 2;
                return token;
            }
            struct Token token = {TOKEN_MULT, 0, t->position + 1, 0.0};
            t->position++;
            return token;
        }
        case '/': {
            struct Token token = {TOKEN_DIV, 0, t->position + 1, 0.0};
            t->position++;
            return token;
        }
        case '(': {
            struct Token token = {TOKEN_PARENTHESES_OPEN, 0, t->position + 1, 0.0};
            t->position++;
            return token;
        }
        case ')': {
            struct Token token = {TOKEN_PARENTHESES_CLOSE, 0, t->position + 1, 0.0};
            t->position++;
            return token;
        }
        case '0' ... '9': {
            // AI refactor: for this block, prompt: "okay, implementiere deine lösung und markiere die änderungen, so wie ich es gemacht habe"
            //- my own digit arithmetic (value * 10 + digit, fraction / scale) was replaced by strtod/strtoll
            //- reason: it overflowed after 18 decimals and rounded twice, so e.g. "8.726 - 8" printed a wrong last digit
            size_t start = t->position;
            // the loops only find where the number ends, the conversion is done below
            while (t->buffer[t->position] >= '0' && t->buffer[t->position] <= '9') {
                t->position++;
            }

            struct Token token;

            // a fraction only starts when "." is directly followed by a digit:
            // ".5", "5." and "1.5e3" therefore fail automatically as an invalid character
            if (t->position < t->length && t->buffer[t->position] == '.' && t->buffer[t->position + 1] >= '0' && t->buffer[t->position + 1] <= '9') {
                t->position++; // skip the dot
                while (t->buffer[t->position] >= '0' && t->buffer[t->position] <= '9') {
                    t->position++;
                }
                // strtod (stdlib) turns the text into the nearest double in one step, so it rounds only once.
                // For "1.5e3" it would also read "e3", but the 'e' is the next token and an
                // invalid character, so that line is an error anyway.
                double number = strtod(t->buffer + start, NULL);
                token = (struct Token){TOKEN_FLOAT, 0, start + 1, number};

            } else {
                // strtoll (stdlib) turns the digits into a long long (base 10)
                long long number = strtoll(t->buffer + start, NULL, 10);
                token = (struct Token){TOKEN_NUMBER, number, start + 1, 0.0};
            }


            return token;
        }
        default: {
            // an invalid character becomes its own token with its own position.
            // The parser reports it as soon as it reaches this token, so the error is at that character.
            struct Token token = {TOKEN_INVALID, 0, t->position + 1, 0.0};
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
// only the first error is stored (has_error). Every function returns as soon
// as it is set, so a later error can never overwrite the position of the first one.
static struct Value primary(struct Tokenizer *t) {
    struct Token token = next_token(t);
   
    // ( opens a group -> is done in the same recursion -> nesting of any depth works
    if (token.type == TOKEN_NUMBER) {
        return make_integer(token.value);
    } else if (token.type == TOKEN_PARENTHESES_OPEN) {
        struct Value value = expression(t, TOKEN_PARENTHESES_CLOSE);
        if (t->has_error) {
            return make_integer(0);
        }
        return value;
    } else if (token.type == TOKEN_FLOAT) {
        return make_float(token.float_value);
    } else {
        // primary() is the only place where a number or '(' is required.
        // The token found here is where parsing cannot continue, so its position is the error:
        // a second operator (7 + * 3 -> at the '*'), an invalid character, ')' in "()",
        // or the '\n' / end of file for a missing operand (3 * -> at the '\n')
        if (t->has_error == 0) { //first ERROR wins
            t->has_error = 1;
            t->error_position = token.pos;
        }
        return make_integer(0);
    }
}

//handles mult and div
//runs inside expression() so 3 + 4 * 5 becomes 3 + (4 * 5)
static struct Value term(struct Tokenizer *t) {
    struct Value solution = power(t);

    if (t->has_error) {
        return make_integer(0);
    }

    while (peek_token(t).type == TOKEN_MULT || peek_token(t).type == TOKEN_DIV) {
        struct Token token = next_token(t);
        // remember where the right operand starts before parsing it,
        // afterwards the tokenizer is already behind it (needed for division by zero)
        long long right_side_pos = peek_token(t).pos;
        struct Value right_side = power(t);

        if (t->has_error) {
                return make_integer(0);
        }

        if (token.type == TOKEN_MULT) {

            if (solution.is_float || right_side.is_float) {
                // same rule as in power(): both operands are valid, only their
                // combination overflows -> error at the operator
                struct Value temp = make_float(as_double(solution) * as_double(right_side));
                
                if (isinf(temp.float_value) || isnan(temp.float_value)) {
                    t->has_error = 1;
                    t->error_position = token.pos;
                }
                solution = temp;
            } else {
                // both operands are ints: the result stays a long long
                solution = make_integer(solution.int_value * right_side.int_value);
            }

        } else if (token.type == TOKEN_DIV) {
            // division by zero is reported at the start of the right operand,
            // because the operand is the wrong part, not the '/'.
            //one check covers int 0 and float 0.0
            if (as_double(right_side) == 0.0) {
                t->has_error = 1;
                t->error_position = right_side_pos;
                return make_integer(0);
            } else if (solution.is_float || right_side.is_float) {
                // same rule as in power(): both operands are valid, only their
                // combination overflows -> error at the operator
                struct Value temp = make_float(as_double(solution) / as_double(right_side));
                if (isinf(temp.float_value) || isnan(temp.float_value)) {
                    t->has_error = 1;
                    t->error_position = token.pos;
                }
                solution = temp;
            } else {
                // both operands are ints: the result stays a long long
                solution = make_integer(solution.int_value / right_side.int_value);
            }
        }
    }

    return solution;
}

//handles power
//rightassociative a ** b ** c is parsed as a ** (b ** c)
// why "**" is right-associative
// For its right side power() calls itself instead of using a loop like term() and
// expression(). So in 2 ** 3 ** 2 the inner call computes 3 ** 2 = 9 first, then 2 ** 9 = 512.
// term() calls power() for its operands, so "**" binds tighter than "*" and "/".
static struct Value power(struct Tokenizer *t) {
    struct Value base = primary(t);
    if (t->has_error) {
        return make_integer(0);
    }

    if (peek_token(t).type != TOKEN_POWER) {
        return base;
    }

    struct Token operator_token = next_token(t);       // the "**" itself
    long long exponent_start_pos = peek_token(t).pos;  // start of the exponent
    struct Value exponent = power(t);                  // calls itself -> right side is grouped first

    if (t->has_error) {
        return make_integer(0);
    }

    if (!base.is_float && !exponent.is_float && exponent.int_value < 0) {
        // int ** negative int is not an integer -> error. It is reported at the
        // start of the exponent, because the exponent is the wrong part.
        // With a float on either side pow() handles it (0.5 ** (0 - 1) = 2)
        t->has_error = 1;
        t->error_position = exponent_start_pos;
        return make_integer(0);
    }

    if (base.is_float || exponent.is_float) {
        struct Value result = make_float(pow(as_double(base), as_double(exponent)));
        if (isinf(result.float_value) || isnan(result.float_value)) {
            // both operands are valid numbers, only their combination is not
            // therefore the ERROR is reported at the operator
            t->has_error = 1;
            t->error_position = operator_token.pos;
            return make_integer(0);
        }
        base = result;
    } else {

        //AI Refactor: suggested handling base 0, 1, -1 separately
        // A huge exponent is only possible for base 0, 1 and -1: every other base
        // would overflow long long after at most 63 steps (tests never overflow).
        // These three bases are handled directly, so the loop always stays short.
        long long result = 1;
        if (base.int_value == 0) {
            if (exponent.int_value == 0) {
                result = 1;
            } else {
                result = 0;
            }
        } else if (base.int_value == 1) {
            result = 1;
        } else if (base.int_value == -1) {
            if (exponent.int_value % 2 == 0) {
                result = 1;
            } else {
                result = -1;
            }
        } else {
            for (long long i = 0; i < exponent.int_value; i++) {
                result *= base.int_value;
            }
        }
        base = make_integer(result);
    }

    return base;
}

//handles addition and subtraction
//leftassociative 
static struct Value expression(struct Tokenizer *t, enum TokenType end_token) {
    struct Value solution = term(t);

    if (t->has_error) {
        return make_integer(0);
    }

    while (peek_token(t).type == TOKEN_PLUS || peek_token(t).type == TOKEN_MINUS) {
        struct Token token = next_token(t);
        struct Value right_side = term(t);

        if (t->has_error) {
            return make_integer(0);
        }

        if (token.type == TOKEN_PLUS) {
            if (solution.is_float || right_side.is_float) {
                // same rule as in power(): both operands are valid, only their
                // combination overflows -> error at the operator
                struct Value temp = make_float(as_double(solution) + as_double(right_side));
                
                if (isinf(temp.float_value) || isnan(temp.float_value)) {
                    t->has_error = 1;
                    t->error_position = token.pos;
                }
                solution = temp;
            } else {
                // both operands are ints: the result stays a long long
                solution = make_integer(solution.int_value + right_side.int_value);
            }

            if (t->has_error) {
                return make_integer(0);
            }
        } else if (token.type == TOKEN_MINUS) {
            if (solution.is_float || right_side.is_float) {
                // same rule as in power(): both operands are valid, only their
                // combination overflows -> error at the operator
                struct Value temp = make_float(as_double(solution) - as_double(right_side));
                
                if (isinf(temp.float_value) || isnan(temp.float_value)) {
                    t->has_error = 1;
                    t->error_position = token.pos;
                }
                solution = temp;
            } else {
                // both operands are ints: the result stays a long long
                solution = make_integer(solution.int_value - right_side.int_value);
            }
            if (t->has_error) {
                return make_integer(0);
            }
        }
    }

    //States which token is allowed to end the expression
    //- for a "normal" line: TOKEN_EOL
    //- for a group: TOKEN_PARENTHSES_CLOSE
    // - for the end of file: TOKEN_EOF
    // any other token means the expression cannot continue here, so its
    // position is the error: an extra ')' on a line (at the ')'), a missing ')' (at the
    // '\n' or end of file), or two numbers in a row (at the second number)
    struct Token end = peek_token(t);
    
    if (end.type == end_token || (end_token == TOKEN_EOL && end.type == TOKEN_EOF)) {
        next_token(t);
        return solution;
    } else {
        if (t->has_error == 0) {
            t->has_error = 1;
            t->error_position = end.pos;
        }
        return make_integer(0);
    }
}

//---- Main ----
//results are collected before writing to the output
//file to ensure that if any line has an error
//only the error is written and no partial results are output
// I used AI (VS Code GitHub Copilot and OpenCode GLM-5.3-Flash) to fit the main method to the current state of the file (the different commits), sessions are not shareable
//- e.g. change var or method names
int main(int arg_count, char **arg_value) {
    const char *dir_argument = NULL;
    const char *out_argument = NULL;
    // AI Refactor
    //memory that main allocates itself
    char *dir_copy = NULL;
    char *default_out = NULL;
    const char *input = NULL;

    for (int i = 1; i < arg_count; i++) {
        if (strcmp(arg_value[i], "-d") == 0 || strcmp(arg_value[i], "--dir") == 0) {
            i++;
            if (i >= arg_count) {
                fprintf(stderr, "Missing argument for -d / --dir\n");
                return 1;
            }
            dir_argument = arg_value[i];
        } else if (strcmp(arg_value[i], "-o") == 0 || strcmp(arg_value[i], "--output-dir") == 0) {
            i++;
            if (i >= arg_count) {
                fprintf(stderr, "Missing argument for -o / --output-dir\n");
                return 1;
            }
            out_argument = arg_value[i];
        } else if (arg_value[i][0] == '-') {
            fprintf(stderr, "Unknown option '%s'\n", arg_value[i]);
            return 1;
        }
        else {
            if (input != NULL) {
                fprintf(stderr, "Multiple input files specified. Only one allowed!\n");
                return 1;
            }
            input = arg_value[i];
        }
    }
    
    if (arg_count == 1) {
        fprintf(stderr, "Usage: %s [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input_file]\n", arg_value[0]);
        return 1;
    }
    if (dir_argument != NULL && input != NULL) {
        fprintf(stderr, "Cannot specify both input file and directory\n");
        return 1;
    }
    if (dir_argument == NULL && input == NULL) {
        fprintf(stderr, "No input file or directory specified\n");
        return 1;
    }

    if (out_argument == NULL) {
        const char *base;
        size_t base_len;

        if (input != NULL) {
            base = file_base_name(input, &base_len);

        } else {
            size_t dir_length = strlen(dir_argument);
            // Remove trailing slash from directory path if present -> would make the last component empty
            if (dir_length > 1 && dir_argument[dir_length - 1] == '/') {
                dir_length--;
                size_t copy_len = dir_length;
                char *copy = malloc(copy_len + 1);

                if (copy == NULL) {
                    fprintf(stderr, "Memory allocation failed\n");
                    return 1;
                }

                // AI Refactor: store a copy of the directory path to free later
                dir_copy = copy;

                strncpy(copy, dir_argument, copy_len);
                copy[copy_len] = '\0';
                dir_argument = copy;
            }

            base = file_base_name(dir_argument, &base_len);

        }

        size_t length = base_len + 1 + strlen(STUDENT_NAME) + 1 +strlen(STUDENT_ID) + 1;

        char *buff = malloc(length);
        if (buff == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            
            //AI Refactor: free any memory allocated for the directory copy before returning
            if (dir_copy != NULL) {
                free(dir_copy);
            }

            return 1;
        }

        // AI Refactor: store the default output buffer to free later
        default_out = buff;

        snprintf(buff, length, "%.*s_%s_%s", (int)base_len, base, STUDENT_NAME, STUDENT_ID);
        out_argument = buff;
    }


    // AI Refactor: suggested freeing main's own allocations
    int result;
    if (dir_argument != NULL) {
        result = process_directory(dir_argument, out_argument);
    } else {
        result = process_file(input, out_argument);
    }

    if (dir_copy != NULL) {
        free(dir_copy);
    }
    if (default_out != NULL) {
        free(default_out);
    }
    return result;
}
