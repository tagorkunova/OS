#include <stdio.h>
#include <stdint.h>

int cas_x86(int *ptr, int expected, int new_val) {
    int result;
    __asm__ volatile(
        "lock cmpxchgl %2, %1"
        : "=a"(result), "+m"(*ptr)
        : "r"(new_val), "0"(expected)
        : "memory"
    );
    return result;
}

void show_cas_behavior() {
    int value = 5;
    int result;

    printf("Initial value: %d\n\n", value);

    printf("CAS(&value, 5, 10):\n");
    result = cas_x86(&value, 5, 10);
    printf("  Returned: %d, Current value: %d\n", result, value);
    printf("  Success: %s\n\n", result == 5 ? "YES" : "NO");

    printf("CAS(&value, 5, 20):\n");
    result = cas_x86(&value, 5, 20);
    printf("  Returned: %d, Current value: %d\n", result, value);
    printf("  Success: %s\n\n", result == 5 ? "YES" : "NO");

    printf("CAS(&value, 10, 20):\n");
    result = cas_x86(&value, 10, 20);
    printf("  Returned: %d, Current value: %d\n", result, value);
    printf("  Success: %s\n", result == 10 ? "YES" : "NO");
}

int main() {
    printf("=== CAS (Compare-And-Swap) x86 Assembly Demo ===\n\n");
    printf("Assembly instruction: lock cmpxchgl\n");
    printf("LOCK prefix ensures atomicity across cores\n\n");
    show_cas_behavior();
    return 0;
}
