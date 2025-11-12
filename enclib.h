// URL handling library
#include "netlib.h"

static void slice(const char *src, char *dst, size_t start, size_t end) {
    if (!src || !dst || start >= end) {
        if (dst) dst[0] = '\0';
        return;
    }
    size_t len = end - start;
    memcpy(dst, src + start, len);
    dst[len] = '\0';
}

char* get_key_from_user(void) {
    static char key[256];
    struct termios old, new;
    size_t i = 0;
    int ch;
    
    // Show prompt
    fprintf(stderr, "Enter encryption/decryption key: ");
    fflush(stderr);
    
    /* Turn echoing off and fail if we can't. */
    if (tcgetattr(STDIN_FILENO, &old) != 0) {
        fprintf(stderr, "ERROR: Could not get terminal attributes\n");
        return NULL;
    }
    
    new = old;
    new.c_lflag &= ~ECHO;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new) != 0) {
        fprintf(stderr, "ERROR: Could not set terminal attributes\n");
        return NULL;
    }
    
    /* Read the password character by character */
    while (i < sizeof(key) - 1) {
        ch = getchar();
        if (ch == EOF || ch == '\n' || ch == '\r') {
            break;
        }
        key[i++] = ch;
    }
    key[i] = '\0';
    
    /* Restore terminal. */
    (void) tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
    
    // Print newline after password input
    fprintf(stderr, "\n");
    
    if (strlen(key) == 0) {
        fprintf(stderr, "Warning: Using empty key\n");
    }
    
    return strdup(key); // Return a copy that can be freed
}

// Base64 implementation
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* base64_encode(const unsigned char* data, size_t input_length, size_t* output_length) {
    *output_length = 4 * ((input_length + 2) / 3);
    char* encoded_data = malloc(*output_length + 1);
    if (!encoded_data) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded_data[j++] = base64_chars[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_chars[triple & 0x3F];
    }

    // Add padding
    switch (input_length % 3) {
        case 1:
            encoded_data[*output_length - 1] = '=';
            encoded_data[*output_length - 2] = '=';
            break;
        case 2:
            encoded_data[*output_length - 1] = '=';
            break;
    }

    encoded_data[*output_length] = '\0';
    return encoded_data;
}

static int base64_char_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

unsigned char* base64_decode(const char* data, size_t input_length, size_t* output_length) {
    if (input_length % 4 != 0) {
        fprintf(stderr, "ERROR: Base64 input length must be multiple of 4\n");
        return NULL;
    }

    // Calculate output length
    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    unsigned char* decoded_data = malloc(*output_length);
    if (!decoded_data) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < input_length;) {
        int sextet_a = base64_char_value(data[i++]);
        int sextet_b = base64_char_value(data[i++]);
        int sextet_c = base64_char_value(data[i++]);
        int sextet_d = base64_char_value(data[i++]);

        if (sextet_a == -1 || sextet_b == -1) {
            free(decoded_data);
            return NULL;
        }

        uint32_t triple = (sextet_a << 18) | (sextet_b << 12) | 
                         ((sextet_c != -1) ? (sextet_c << 6) : 0) | 
                         ((sextet_d != -1) ? sextet_d : 0);

        if (j < *output_length) decoded_data[j++] = (triple >> 16) & 0xFF;
        if (j < *output_length && sextet_c != -1) decoded_data[j++] = (triple >> 8) & 0xFF;
        if (j < *output_length && sextet_d != -1) decoded_data[j++] = triple & 0xFF;
    }

    return decoded_data;
}

// Base85 implementation (Ascii85)
char* base85_encode(const unsigned char* data, size_t input_length, size_t* output_length) {
    if (input_length == 0) {
        *output_length = 0;
        char* empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    // Calculate output size: 5 output chars for every 4 input bytes
    *output_length = ((input_length + 3) / 4) * 5;
    char* encoded_data = malloc(*output_length + 1);
    if (!encoded_data) return NULL;

    size_t i = 0, j = 0;
    while (i < input_length) {
        uint32_t value = 0;
        int bytes = 0;

        // Read up to 4 bytes
        for (int k = 0; k < 4; k++) {
            value = (value << 8) | (i < input_length ? data[i++] : 0);
            if (i <= input_length) bytes++;
        }

        // Special case: 4 zero bytes become 'z'
        if (bytes == 4 && value == 0) {
            encoded_data[j++] = 'z';
            continue;
        }

        // Convert 32-bit value to 5 base85 digits
        char digits[5];
        for (int k = 4; k >= 0; k--) {
            digits[k] = value % 85;
            value /= 85;
        }

        // Output the digits
        for (int k = 0; k < 5; k++) {
            encoded_data[j++] = digits[k] + 33; // Map to printable ASCII
        }
    }

    *output_length = j;
    encoded_data[j] = '\0';
    return encoded_data;
}

unsigned char* base85_decode(const char* data, size_t input_length, size_t* output_length) {
    if (input_length == 0) {
        *output_length = 0;
        unsigned char* empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    // Calculate maximum output size
    *output_length = (input_length * 4 + 4) / 5;
    unsigned char* decoded_data = malloc(*output_length);
    if (!decoded_data) return NULL;

    size_t i = 0, j = 0;
    while (i < input_length) {
        if (data[i] == 'z') {
            // Handle 'z' shortcut for 4 zero bytes
            if (j + 4 > *output_length) {
                free(decoded_data);
                return NULL;
            }
            decoded_data[j++] = 0;
            decoded_data[j++] = 0;
            decoded_data[j++] = 0;
            decoded_data[j++] = 0;
            i++;
            continue;
        }

        uint32_t value = 0;
        int digits = 0;

        // Read exactly 5 base85 digits
        for (int k = 0; k < 5 && i < input_length; k++, i++) {
            if (data[i] == 'z') {
                // Can't have 'z' in middle of group
                free(decoded_data);
                return NULL;
            }
            if (data[i] < 33 || data[i] > 117) {
                free(decoded_data);
                return NULL;
            }
            value = value * 85 + (data[i] - 33);
            digits++;
        }

        // We should have exactly 5 digits unless at end
        if (digits != 5 && i < input_length) {
            free(decoded_data);
            return NULL;
        }

        // Convert to bytes (we may have padding at the end)
        int bytes = (digits == 5) ? 4 : (digits - 1);
        for (int k = 3; k >= 0; k--) {
            if (k < bytes) {
                if (j >= *output_length) {
                    free(decoded_data);
                    return NULL;
                }
                decoded_data[j++] = (value >> (k * 8)) & 0xFF;
            }
        }
    }

    *output_length = j;
    return decoded_data;
}

void xor_with_key(unsigned char* data, size_t data_len, const char* key) {
    size_t key_len = strlen(key);
    if (key_len == 0) return;
    
    for (size_t i = 0; i < data_len; i++) {
        data[i] ^= key[i % key_len];
    }
}

void xor_with_inverse_key(unsigned char* data, size_t data_len, const char* key) {
    size_t key_len = strlen(key);
    if (key_len == 0) return;
    
    for (size_t i = 0; i < data_len; i++) {
        data[i] ^= ~key[i % key_len];  // Inverse is bitwise NOT
    }
}

char* encrypt_string(const char* input, const char* key) {
    size_t input_len = strlen(input);
    if (input_len == 0) {
        char* empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    
    printf("Encrypting %zu bytes...\n", input_len);
    
    // Step 1: XOR with user-provided key
    unsigned char* step1 = malloc(input_len);
    if (!step1) return NULL;
    memcpy(step1, input, input_len);
    xor_with_key(step1, input_len, key);
    
    // Step 2: base85 encode
    size_t step2_len;
    char* step2 = base85_encode(step1, input_len, &step2_len);
    free(step1);
    if (!step2) return NULL;
    printf("After Base85: %zu bytes\n", step2_len);
    
    // Step 3: XOR with inverse of user-provided key
    xor_with_inverse_key((unsigned char*)step2, step2_len, key);
    
    // Step 4: base64 encode
    size_t step4_len;
    char* step4 = base64_encode((unsigned char*)step2, step2_len, &step4_len);
    free(step2);
    if (step4) {
        printf("Final encrypted: %zu bytes\n", step4_len);
    }
    
    return step4;
}

char* decrypt_string(const char* input, const char* key) {
    size_t input_len = strlen(input);
    if (input_len == 0) {
        char* empty = malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    
    printf("Decrypting %zu bytes...\n", input_len);
    
    // Reverse step 4: base64 decode
    size_t step1_len;
    unsigned char* step1 = base64_decode(input, input_len, &step1_len);
    if (!step1) {
        fprintf(stderr, "Base64 decode failed\n");
        return NULL;
    }
    printf("After Base64 decode: %zu bytes\n", step1_len);
    
    // Reverse step 3: XOR with inverse of user-provided key
    xor_with_inverse_key(step1, step1_len, key);
    
    // Reverse step 2: base85 decode
    size_t step2_len;
    unsigned char* step2 = base85_decode((char*)step1, step1_len, &step2_len);
    free(step1);
    if (!step2) {
        fprintf(stderr, "Base85 decode failed\n");
        return NULL;
    }
    printf("After Base85 decode: %zu bytes\n", step2_len);
    
    // Reverse step 1: XOR with user-provided key
    xor_with_key(step2, step2_len, key);
    
    // Convert back to string
    char* result = malloc(step2_len + 1);
    if (!result) {
        free(step2);
        return NULL;
    }
    memcpy(result, step2, step2_len);
    result[step2_len] = '\0';
    free(step2);
    
    printf("Final decrypted: %zu bytes\n", step2_len);
    return result;
}

// ============================================================================
// File I/O
// ============================================================================

static char *read_entire_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: Could not open file '%s'\n", filename);
        return NULL;
    }
    
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "ERROR: Could not seek in file '%s'\n", filename);
        fclose(f);
        return NULL;
    }
    
    long size = ftell(f);
    if (size < 0) {
        fprintf(stderr, "ERROR: Could not get file size for '%s'\n", filename);
        fclose(f);
        return NULL;
    }
    
    rewind(f);  // Faster than fseek to beginning
    
    char *content = malloc(size + 1);
    if (!content) {
        fprintf(stderr, "ERROR: Out of memory (file size: %ld bytes)\n", size);
        fclose(f);
        return NULL;
    }
    
    size_t read_bytes = fread(content, 1, size, f);
    content[read_bytes] = '\0';
    fclose(f);
    
    return content;
}

// Unified function to get source code from either file or URL
char* get_source_code(const char* source_path) {
    if (is_url(source_path)) {
        printf("Downloading from URL: %s\n", source_path);
        return download_from_url(source_path);
    } else {
        printf("Reading local file: %s\n", source_path);
        return read_entire_file(source_path);
    }
}

char* read_enc_dec_managed(char* first_arg, char* second_arg, int argc, int* encryption_mode_p, char* source_path_p){

    // encryption_mode -1 = normal, 0 = decrypt, 1 = encrypt
    int encryption_mode = (int)*encryption_mode_p;
    char *source_path = source_path_p;
    char* source_code = NULL;

    // Check if first argument is encryption mode flag
    if (strcmp(first_arg, "1") == 0) {
        encryption_mode = 1;
        if (argc < 3) {
            fprintf(stderr, "ERROR: encryption mode requires a file path\n");
            exit(1);
        }
        source_path = second_arg;
    } else if (strcmp(first_arg, "0") == 0) {
        encryption_mode = 0;
        if (argc < 3) {
            fprintf(stderr, "ERROR: decryption mode requires a file path\n");
            exit(1);
        }
        source_path = second_arg;
    } else {
        encryption_mode = -1;
        source_path = first_arg;
    }

    // Get source code (from file or URL)
    source_code = get_source_code(source_path);
    if (!source_code) {
        fprintf(stderr, "ERROR: Could not retrieve source code from: %s\n", source_path);
        exit(1);
    }

    // Handle encryption if requested
    if (encryption_mode == 1) {
        // Only allow encryption of local files (not URLs)
        if (is_url(source_path)) {
            fprintf(stderr, "ERROR: Cannot encrypt URLs directly. Encrypt target file with this program before attempting to retrieve it.\n");
            free(source_code);
            exit(1);
        }
        
        printf("Encrypting file: %s\n", source_path);
        char* key = get_key_from_user();
        if (!key) {
            fprintf(stderr, "ERROR: Failed to get encryption key\n");
            free(source_code);
            exit(1);
        }
        
        char* encrypted = encrypt_string(source_code, key);
        free(key);
        
        if (!encrypted) {
            fprintf(stderr, "ERROR: Encryption failed\n");
            exit(1);
        }
        
        // Write encrypted content back to new file
        char source_path_new[strlen(source_path)+4];
        memset(source_path_new, 0, strlen(source_path)+4);

        slice(source_path, source_path_new, 0, strlen(source_path)-2);
        FILE* fp = fopen(strcat(source_path_new, "_enc.c"), "wb");
        if (!fp) {
            fprintf(stderr, "ERROR: Could not write encrypted file\n");
            free(encrypted);
            exit(1);
        }
        
        fwrite(encrypted, strlen(encrypted), 1, fp);
        fclose(fp);
        free(encrypted);
        
        printf("File encrypted successfully: %s\n", source_path_new);
        exit(0);
    }

    // Handle decryption if requested
    if (encryption_mode == 0) {
        
        printf("Decrypting file: %s\n", source_path);
        char* key = get_key_from_user();
        if (!key) {
            fprintf(stderr, "ERROR: Failed to get decryption key\n");
            free(source_code);
            exit(1);
        }
        
        char* decrypted = decrypt_string(source_code, key);
        free(key);
        
        if (!decrypted) {
            fprintf(stderr, "ERROR: Decryption failed - invalid key or corrupted file\n");
            exit(1);
        }
        
        printf("Decrypted successfully, length: %zu bytes\n", strlen(decrypted));
        
        // Use decrypted content for compilation
        source_code = decrypted;
    }
    return source_code;
}