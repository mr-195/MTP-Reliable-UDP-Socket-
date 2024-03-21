#include <stdio.h>


struct test{
    char str[10];
    int data;
};


struct test retfun(){
    struct test ad={
        .str="Hello from retfun",
        .data=2
    };
    return ad;
}

int main(){
    struct test ret=retfun();
    printf("%d %s",ret.data,ret.str);
    return 0;
}