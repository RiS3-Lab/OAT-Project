#include <stdio.h>
#include "cfv_bellman.h"

// data type test 
char c = 'c';
char *pc = NULL;
char parr[20] = "test char array";
unsigned char uc = 10;
//short sh = 0;
//int a = 3;
//int arr[3] = {0};
//int *pa = NULL;
//int **ppa = NULL;

// derived data type test
typedef struct foo {
    int m_int;
    char m_c;
    char m_d;
    char *m_cp;
    char m_carr[10];
    int *m_intp;
} foo_t;

foo_t f;

int add(int a, int b) {
    return a + b;
}

int (*fadd)(int, int);

int main() {
  int a = 10;
  char d = 'a';
  foo_t g, *pfoo;

  cfv_init();

  //define, redefine, use, pointer write, test
  c = 'd';
  parr[3] = c;
  parr[4] = c;
  d = c;
  c = 'e';
  pc = &c;
  *pc = 'f';

  pfoo = &g;
  pfoo->m_int = 10;

  f.m_int = add(10, a);
  f.m_c = d;
  f.m_carr[3] = d;
  f.m_intp = &a;
  f.m_cp = &d;
//
  g.m_int = add(10, a);
  g.m_carr[5] = d;
  g.m_c = d;
  g.m_intp = &a;
  g.m_cp = &d;

  switch(c) {
     case 'a':
         printf("c = a\n");
         break;
     case 'b':
         printf("c = b\n");
         break;
     case 'c':
         printf("c = c\n");
         break;
     case 'd':
         printf("c = d\n");
         break;
     default:
         printf("c = default\n");
         break;
  }

  while ( f.m_int < 30) {
  for ( a = 0; a < 10; a++) {
     f.m_int ++;
     uc++;
  }
  for ( a = 0; a < 5; a++) {
     f.m_int ++;
     uc++;
  }

  f.m_int ++;
 }

  fadd = add;
  a = (*fadd)(3,5);

  uc = 20;

  printf("%c, %u %s f.m_c:%c\n", c, (unsigned int)uc, parr, f.m_c);

  cfv_quote();
  return 0;
}
