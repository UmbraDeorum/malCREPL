// Check if the path is a URL
int is_url(const char* path) {
    return (strncmp(path, "http://", 7) == 0 || 
            strncmp(path, "https://", 8) == 0);
}

// Struct to store downloaded data
typedef struct {
    char *data;
    size_t size;
} MemoryBuffer;

// Callback function for libcurl to write data to memory
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    MemoryBuffer *buffer = (MemoryBuffer *)userp;

    // Reallocate buffer to accommodate new data
    char *new_data = realloc(buffer->data, buffer->size + total_size + 1);
    if (!new_data) {
        fprintf(stderr, "ERROR: Memory reallocation failed (%zu + %zu bytes)\n", 
                buffer->size, total_size);
        return 0; // Return 0 to indicate failure to libcurl
    }

    buffer->data = new_data;
    memcpy(buffer->data + buffer->size, contents, total_size);
    buffer->size += total_size;
    buffer->data[buffer->size] = '\0'; // Always null-terminate

    return total_size;
}

// Initialize memory buffer
static int init_memory_buffer(MemoryBuffer *buffer) {
    buffer->data = malloc(1); // Start with minimal allocation
    if (!buffer->data) {
        fprintf(stderr, "ERROR: Initial memory allocation failed\n");
        return -1;
    }
    buffer->data[0] = '\0';
    buffer->size = 0;
    return 0;
}

// Download content from URL using libcurl
char* download_from_url(const char* url) {
    CURL *curl = NULL;
    CURLcode res;
    MemoryBuffer buffer = {0};
    char *result = NULL;

    // Validate input
    if (!url || strlen(url) == 0) {
        fprintf(stderr, "ERROR: Invalid URL provided\n");
        return NULL;
    }

    printf("Downloading from: %s\n", url);

    // Initialize memory buffer
    if (init_memory_buffer(&buffer) != 0) {
        return NULL;
    }

    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (!curl) {
        fprintf(stderr, "ERROR: Failed to initialize CURL\n");
        goto cleanup;
    }

    // Configure curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buffer);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "malcrepl/1.0");
    
    // Security: Ignore SSL certificate verification (for self-signed certs)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    // Performance and reliability options
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); // Fail on HTTP errors (4xx, 5xx)
    
    // Optional: Enable for debugging
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    // Execute the request
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "ERROR: Download failed: %s\n", curl_easy_strerror(res));
        
        // Provide more specific error messages for common cases
        if (res == CURLE_COULDNT_CONNECT) {
            fprintf(stderr, "Hint: Check if the server is running and accessible\n");
        } else if (res == CURLE_SSL_CONNECT_ERROR) {
            fprintf(stderr, "Hint: SSL/TLS connection issue - check certificate configuration\n");
        } else if (res == CURLE_OPERATION_TIMEDOUT) {
            fprintf(stderr, "Hint: Connection timed out - check network connectivity\n");
        }
        
        goto cleanup;
    }

    // Verify we actually got data
    if (buffer.size == 0) {
        fprintf(stderr, "ERROR: Empty response received from server\n");
        goto cleanup;
    }

    printf("Downloaded %zu bytes successfully\n", buffer.size);
    
    // Return the data (transfer ownership to caller)
    result = buffer.data;
    buffer.data = NULL; // Prevent double-free in cleanup

cleanup:
    // Cleanup libcurl resources
    if (curl) {
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    
    // Cleanup buffer if not transferred to result
    if (buffer.data) {
        free(buffer.data);
    }
    
    return result;
}

// Optional: Function to get HTTP status code (for debugging)
int get_http_status(const char* url) {
    CURL *curl;
    CURLcode res;
    long http_code = 0;
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }
        
        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    return (int)http_code;
}