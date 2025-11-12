#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================================
// Basic Examples
// ============================================================================

void hello(void) {
    printf("Hello, World!\n");
}

void greet(char *name) {
    printf("Hello, %s! Welcome to the C REPL.\n", name);
}

void echo(char *message) {
    printf("You said: %s\n", message);
}

// ============================================================================
// Return Examples
// ============================================================================

int add_ret(int a, int b) {
    return (a + b);
}

char* echo_ret(char *message) {
    return message;
}

char char_ret(char character) {
    return character;
}

double divide_ret(double a, double b) {
    return a / b;
}

// ============================================================================
// Math Examples
// ============================================================================

void add(int a, int b) {
    printf("%d + %d = %d\n", a, b, a + b);
}

void subtract(int a, int b) {
    printf("%d - %d = %d\n", a, b, a - b);
}

void multiply(int a, int b) {
    printf("%d × %d = %d\n", a, b, a * b);
}

void divide(double a, double b) {
    if (b == 0.0) {
        printf("Error: Division by zero!\n");
    } else {
        printf("%.2f ÷ %.2f = %.2f\n", a, b, a / b);
    }
}

void power(double base, int exponent) {
    double result = pow(base, exponent);
    printf("%.2f ^ %d = %.2f\n", base, exponent, result);
}

void sqrt_demo(double x) {
    if (x < 0) {
        printf("Error: Cannot take square root of negative number\n");
    } else {
        printf("√%.2f = %.2f\n", x, sqrt(x));
    }
}

void factorial(int n) {
    if (n < 0) {
        printf("Error: Factorial undefined for negative numbers\n");
        return;
    }
    
    long long result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    printf("%d! = %lld\n", n, result);
}

// ============================================================================
// String Examples
// ============================================================================

void length(char *str) {
    printf("Length of \"%s\" is %zu\n", str, strlen(str));
}

void reverse(char *str) {
    size_t len = strlen(str);
    char *reversed = malloc(len + 1);
    
    for (size_t i = 0; i < len; i++) {
        reversed[i] = str[len - 1 - i];
    }
    reversed[len] = '\0';
    
    printf("Original: %s\n", str);
    printf("Reversed: %s\n", reversed);
    
    free(reversed);
}

void uppercase(char *str) {
    printf("Original: %s\n", str);
    printf("Uppercase: ");
    for (size_t i = 0; str[i]; i++) {
        putchar(toupper((unsigned char)str[i]));
    }
    printf("\n");
}

void lowercase(char *str) {
    printf("Original: %s\n", str);
    printf("Lowercase: ");
    for (size_t i = 0; str[i]; i++) {
        putchar(tolower((unsigned char)str[i]));
    }
    printf("\n");
}

void count_vowels(char *str) {
    int count = 0;
    for (size_t i = 0; str[i]; i++) {
        char c = tolower((unsigned char)str[i]);
        if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
            count++;
        }
    }
    printf("Vowels in \"%s\": %d\n", str, count);
}

void repeat(char *str, int times) {
    printf("Repeating \"%s\" %d times:\n", str, times);
    for (int i = 0; i < times; i++) {
        printf("%s", str);
        if (i < times - 1) printf(", ");
    }
    printf("\n");
}

// ============================================================================
// Number Theory Examples
// ============================================================================

void is_prime(int n) {
    if (n < 2) {
        printf("%d is not prime\n", n);
        return;
    }
    
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) {
            printf("%d is not prime (divisible by %d)\n", n, i);
            return;
        }
    }
    
    printf("%d is prime\n", n);
}

void gcd(int a, int b) {
    int original_a = a;
    int original_b = b;
    
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    
    printf("GCD(%d, %d) = %d\n", original_a, original_b, a);
}

void fibonacci(int n) {
    if (n < 1) {
        printf("Error: n must be positive\n");
        return;
    }
    
    printf("First %d Fibonacci numbers:\n", n);
    long long a = 0, b = 1;
    
    for (int i = 0; i < n; i++) {
        printf("%lld", a);
        if (i < n - 1) printf(", ");
        
        long long next = a + b;
        a = b;
        b = next;
    }
    printf("\n");
}

// ============================================================================
// Conversion Examples
// ============================================================================

void celsius_to_fahrenheit(double celsius) {
    double fahrenheit = (celsius * 9.0 / 5.0) + 32.0;
    printf("%.2f°C = %.2f°F\n", celsius, fahrenheit);
}

void fahrenheit_to_celsius(double fahrenheit) {
    double celsius = (fahrenheit - 32.0) * 5.0 / 9.0;
    printf("%.2f°F = %.2f°C\n", fahrenheit, celsius);
}

void miles_to_km(double miles) {
    double km = miles * 1.60934;
    printf("%.2f miles = %.2f km\n", miles, km);
}

void kg_to_pounds(double kg) {
    double pounds = kg * 2.20462;
    printf("%.2f kg = %.2f pounds\n", kg, pounds);
}

// ============================================================================
// Fun Examples
// ============================================================================

void countdown(int n) {
    printf("Counting down from %d:\n", n);
    for (int i = n; i >= 0; i--) {
        printf("%d... ", i);
        fflush(stdout);
    }
    printf("Blast off! 🚀\n");
}

void draw_triangle(int height) {
    printf("Triangle with height %d:\n", height);
    for (int i = 1; i <= height; i++) {
        for (int j = 0; j < height - i; j++) {
            printf(" ");
        }
        for (int j = 0; j < 2 * i - 1; j++) {
            printf("*");
        }
        printf("\n");
    }
}

void draw_square(int size) {
    printf("Square with size %d:\n", size);
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (i == 0 || i == size - 1 || j == 0 || j == size - 1) {
                printf("* ");
            } else {
                printf("  ");
            }
        }
        printf("\n");
    }
}

void print_ascii(char c) {
    printf("Character: '%c'\n", c);
    printf("ASCII code: %d\n", (int)c);
    printf("Hexadecimal: 0x%02X\n", (unsigned char)c);
    printf("Binary: ");
    for (int i = 7; i >= 0; i--) {
        printf("%d", (c >> i) & 1);
        if (i == 4) printf(" ");
    }
    printf("\n");
}

// ============================================================================
// Statistical Examples
// ============================================================================

void sum_range(int start, int end) {
    if (start > end) {
        printf("Error: start must be <= end\n");
        return;
    }
    
    long long sum = 0;
    for (int i = start; i <= end; i++) {
        sum += i;
    }
    
    printf("Sum of integers from %d to %d = %lld\n", start, end, sum);
}

void average(double a, double b, double c) {
    double avg = (a + b + c) / 3.0;
    printf("Average of %.2f, %.2f, %.2f = %.2f\n", a, b, c, avg);
}

// ============================================================================
// Help Function
// ============================================================================

void list_functions(void) {
    printf("\n=== Available Functions ===\n\n");
    
    printf("Basic:\n");
    printf("  hello()                    - Print hello world\n");
    printf("  greet(name)                - Greet someone\n");
    printf("  echo(message)              - Echo a message\n\n");
    
    printf("Math:\n");
    printf("  add(a, b)                  - Add two integers\n");
    printf("  subtract(a, b)             - Subtract two integers\n");
    printf("  multiply(a, b)             - Multiply two integers\n");
    printf("  divide(a, b)               - Divide two doubles\n");
    printf("  power(base, exp)           - Calculate power\n");
    printf("  sqrt_demo(x)               - Calculate square root\n");
    printf("  factorial(n)               - Calculate factorial\n\n");
    
    printf("String:\n");
    printf("  length(str)                - Get string length\n");
    printf("  reverse(str)               - Reverse a string\n");
    printf("  uppercase(str)             - Convert to uppercase\n");
    printf("  lowercase(str)             - Convert to lowercase\n");
    printf("  count_vowels(str)          - Count vowels\n");
    printf("  repeat(str, times)         - Repeat string\n\n");
    
    printf("Number Theory:\n");
    printf("  is_prime(n)                - Check if prime\n");
    printf("  gcd(a, b)                  - Greatest common divisor\n");
    printf("  fibonacci(n)               - First n Fibonacci numbers\n\n");
    
    printf("Conversions:\n");
    printf("  celsius_to_fahrenheit(c)   - Temperature conversion\n");
    printf("  fahrenheit_to_celsius(f)   - Temperature conversion\n");
    printf("  miles_to_km(miles)         - Distance conversion\n");
    printf("  kg_to_pounds(kg)           - Weight conversion\n\n");
    
    printf("Fun:\n");
    printf("  countdown(n)               - Countdown from n\n");
    printf("  draw_triangle(height)      - Draw ASCII triangle\n");
    printf("  draw_square(size)          - Draw ASCII square\n");
    printf("  print_ascii(c)             - Show ASCII info\n\n");
    
    printf("Statistics:\n");
    printf("  sum_range(start, end)      - Sum of integer range\n");
    printf("  average(a, b, c)           - Average of three numbers\n\n");
}