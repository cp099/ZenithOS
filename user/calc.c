#include "libc.h"

#define SCALE 1000

int parse_error = 0;

char* skip_spaces(char* p) {
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

int parse_scaled_int(char** p) {
    char* s = *p;
    s = skip_spaces(s);
    int is_neg = 0;
    if (*s == '-') {
        is_neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    
    int int_part = 0;
    int has_digits = 0;
    while (*s >= '0' && *s <= '9') {
        int_part = int_part * 10 + (*s - '0');
        s++;
        has_digits = 1;
    }
    
    int frac_part = 0;
    if (*s == '.') {
        s++;
        int multiplier = SCALE / 10;
        while (*s >= '0' && *s <= '9') {
            if (multiplier > 0) {
                frac_part += (*s - '0') * multiplier;
                multiplier /= 10;
            }
            s++;
            has_digits = 1;
        }
    }
    
    if (!has_digits) {
        parse_error = 1;
    }
    *p = s;
    
    int total_val = int_part * SCALE + frac_part;
    return is_neg ? -total_val : total_val;
}

void print_scaled_int(int val) {
    if (val < 0) {
        putchar('-');
        val = -val;
    }
    
    int int_part = val / SCALE;
    int frac_part = val % SCALE;
    
    puts(itoa(int_part, 10));
    
    if (frac_part == 0) {
        puts(".0");
    } else {
        putchar('.');
        if (frac_part < 10) {
            puts("00");
        } else if (frac_part < 100) {
            puts("0");
        }
        
        char* frac_str = itoa(frac_part, 10);
        int len = strlen(frac_str);
        while (len > 0 && frac_str[len - 1] == '0') {
            len--;
        }
        for (int i = 0; i < len; i++) {
            putchar(frac_str[i]);
        }
    }
}

int div64_32(long long dividend, int divisor) {
    if (divisor == 0) {
        puts("Error: Division by zero!\n\n");
        parse_error = 1;
        return 0;
    }

    long long limit_pos = 2147483647LL;
    long long limit_neg = -2147483648LL;
    int overflow = 0;

    if (divisor > 0) {
        if (dividend > limit_pos * divisor || dividend < limit_neg * divisor) {
            overflow = 1;
        }
    } else {
        if (dividend < limit_pos * divisor || dividend > limit_neg * divisor) {
            overflow = 1;
        }
    }

    if (overflow) {
        puts("Error: Integer overflow in division.\n\n");
        parse_error = 1;
        return 0;
    }

    int quotient;
    int remainder;
    unsigned int low = (unsigned int)dividend;
    unsigned int high = (unsigned int)(dividend >> 32);
    __asm__ (
        "idivl %4"
        : "=a"(quotient), "=d"(remainder)
        : "a"(low), "d"(high), "rm"(divisor)
    );
    return quotient;
}

int expression(char** p);
int term(char** p);
int factor(char** p);

int factor(char** p) {
    *p = skip_spaces(*p);
    char c = **p;

    if (c == '(') {
        (*p)++;
        int val = expression(p);
        *p = skip_spaces(*p);
        if (**p == ')') {
            (*p)++;
        } else {
            puts("Error: Mismatched parentheses.\n\n");
            parse_error = 1;
        }
        return val;
    }

    if (c == '-') {
        (*p)++;
        return -factor(p);
    }
    if (c == '+') {
        (*p)++;
        return factor(p);
    }

    return parse_scaled_int(p);
}

int term(char** p) {
    int val = factor(p);
    if (parse_error) return 0;

    while (1) {
        *p = skip_spaces(*p);
        char op = **p;
        if (op == '*' || op == '/' || op == '%') {
            (*p)++;
            int next_val = factor(p);
            if (parse_error) return 0;

            if (op == '*') {
                long long prod = (long long)val * next_val;
                val = div64_32(prod, SCALE);
            } else if (op == '/') {
                if (next_val == 0) {
                    puts("Error: Division by zero!\n\n");
                    parse_error = 1;
                    return 0;
                }
                long long numer = (long long)val * SCALE;
                val = div64_32(numer, next_val);
            } else if (op == '%') {
                if (next_val == 0) {
                    puts("Error: Division by zero!\n\n");
                    parse_error = 1;
                    return 0;
                }
                if (val == -2147483648 && next_val == -1) {
                    val = 0;
                } else {
                    val %= next_val;
                }
            }
        } else {
            break;
        }
    }
    return val;
}

int expression(char** p) {
    int val = term(p);
    if (parse_error) return 0;

    while (1) {
        *p = skip_spaces(*p);
        char op = **p;
        if (op == '+' || op == '-') {
            (*p)++;
            int next_val = term(p);
            if (parse_error) return 0;

            if (op == '+') {
                val += next_val;
            } else if (op == '-') {
                val -= next_val;
            }
        } else {
            break;
        }
    }
    return val;
}

void print_help() {
    puts("+----------------------------------------------+\n");
    puts("|                 ZENITH_CALC                  |\n");
    puts("|      Interactive Calculator for Zenith OS    |\n");
    puts("+----------------------------------------------+\n");
    puts("Commands:\n");
    puts("  exit / quit       - Return to Zenith OS shell\n");
    puts("  clear             - Clear the screen\n");
    puts("  help              - Show this instruction card\n\n");
    puts("Operations:\n");
    puts("  Addition (+), Subtraction (-), Multiplication (*)\n");
    puts("  Division (/), Modulus (%)\n\n");
    puts("Examples:\n");
    puts("  3.5 * 2           - Decimals supported (evaluates to 7.0)\n");
    puts("  (10 + 2.5) * 4    - Parentheses (evaluates to 50.0)\n");
    puts("  * 3 + 1.5         - Accumulator mode (last_ans * 3 + 1.5)\n\n");
}

int main(void) {
    clear();
    print_help();

    int last_result = 0;
    int has_last_result = 0;
    char input_buf[80];

    while (1) {
        if (has_last_result) {
            puts("[ans = ");
            print_scaled_int(last_result);
            puts("] zenith_calc> ");
        } else {
            puts("zenith_calc> ");
        }

        int input_len = input(input_buf, sizeof(input_buf));
        if (input_len > 0 && input_buf[input_len - 1] == '\n') {
            input_buf[input_len - 1] = '\0';
        }

        char* p = input_buf;
        p = skip_spaces(p);

        if (*p == '\0') {
            continue;
        }

        if (strcmp(p, "exit") == 0 || strcmp(p, "quit") == 0) {
            clear();
            puts("zenith_calc: closed. Returning to shell...\n");
            exit();
            return 0;
        }

        if (strcmp(p, "help") == 0) {
            print_help();
            continue;
        }

        if (strcmp(p, "clear") == 0) {
            clear();
            print_help();
            continue;
        }

        int is_acc = 0;
        if (has_last_result) {
            char op = *p;
            if (op == '+' || op == '-' || op == '*' || op == '/' || op == '%') {
                if (op == '*' || op == '/' || op == '%') {
                    is_acc = 1;
                } else {
                    if (p[1] == ' ' || p[1] == '\t' || p[1] == '\0') {
                        is_acc = 1;
                    } else {
                        char* temp_p = p + 1;
                        parse_error = 0;
                        parse_scaled_int(&temp_p);
                        temp_p = skip_spaces(temp_p);
                        if (*temp_p == '\0' && !parse_error) {
                            is_acc = 1;
                        }
                    }
                }
            }
        }

        char expr_buf[120];
        char* expr = p;

        if (is_acc) {
            // Format last_result as a decimal string (e.g. 3500 -> 3.500)
            int temp_val = last_result;
            int is_neg = 0;
            if (temp_val < 0) {
                is_neg = 1;
                temp_val = -temp_val;
            }
            int int_part = temp_val / SCALE;
            int frac_part = temp_val % SCALE;
            
            int idx = 0;
            if (is_neg) {
                expr_buf[idx++] = '-';
            }
            
            char* int_str = itoa(int_part, 10);
            int k = 0;
            while (int_str[k]) {
                expr_buf[idx++] = int_str[k++];
            }
            
            expr_buf[idx++] = '.';
            
            // Format fractional part with leading zeros
            if (frac_part < 10) {
                expr_buf[idx++] = '0';
                expr_buf[idx++] = '0';
            } else if (frac_part < 100) {
                expr_buf[idx++] = '0';
            }
            
            char* frac_str = itoa(frac_part, 10);
            k = 0;
            while (frac_str[k]) {
                expr_buf[idx++] = frac_str[k++];
            }
            
            int j = 0;
            while (p[j]) {
                expr_buf[idx++] = p[j++];
            }
            expr_buf[idx] = '\0';
            expr = expr_buf;
        }

        parse_error = 0;
        char* eval_ptr = expr;
        int result = expression(&eval_ptr);
        eval_ptr = skip_spaces(eval_ptr);

        if (*eval_ptr != '\0') {
            puts("Error: Invalid expression syntax.\n\n");
            continue;
        }

        if (parse_error) {
            continue;
        }

        last_result = result;
        has_last_result = 1;
        puts("= ");
        print_scaled_int(last_result);
        puts("\n\n");
    }

    return 0;
}
