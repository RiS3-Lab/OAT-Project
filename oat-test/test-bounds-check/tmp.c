#include <stdio.h>
struct abc {
int i;
int j;
int arr[10];
} a = {1,3, {1,3,3,5,3,3,3,3,3,3}};

int f(struct abc b) {
    printf("abc-a[3]:%d\n", b.arr[3]);
    b.arr[3]  = 9;
    return 0;
}

int main() {
 f(a);
 f(a);
    return 0;
}
