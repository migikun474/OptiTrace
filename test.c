#include <stdio.h>

int square(int x) {
    return x * x;
}

int add(int a, int b) {
    return a + b;
}

int main() {
    int x = square(5);
    int y = add(x, 10);

    printf("%d\n", y);

    return 0;
}