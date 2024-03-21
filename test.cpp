#include <stdio.h>

struct temp
{
    int a, b;
    void print()
    {
        printf("%d->%d\n", a, b);
    }
};

int main()
{
    struct temp cur;
    cur.a = 2;
    cur.b = 3;
    cur.print();
    return 0;
}