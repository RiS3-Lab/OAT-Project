#include<stdio.h>
#include<stdlib.h>

int __attribute__((annotate("sensitive"))) abc = 8;
int __attribute__((annotate("sensitive"))) garr[100];
int main(int argc, char** argv){

  int __attribute__((annotate("sensitive"))) arr[100];
  int __attribute__((annotate("sensitive"))) *a;
  int num, i, j;

  size_t result = 0;
  size_t gresult = 0;

  a = arr;

  for(i = 0; i < 100; i++){
    arr[i] = (i * 100 +2) % 102;
    garr[i] = (i * 100 +2) % 102;
  }

  printf("enter a number to test local array:\n");
  scanf("%d", &num);

  
  for(j= 0; j< 100; j++){
    for(i = 0; i< num; i++){
      result = result + arr[i];
    }
  }

  printf("enter a number to test global array:\n");
  scanf("%d", &num);

  for(j= 0; j< 100; j++){
    for(i = 0; i< num; i++){
      gresult = gresult + garr[i];
    }
  }


  printf("enter a number to test pointer access global array:\n");
  scanf("%d", &num);

  *(a+num) = 300;
  printf("test pointer a: *(a+num) = %d\n", *(a+num)); 
 
  printf("the num is %d\n", num);
  printf("the result is %zd\n", result);
  printf("the gresult is %zd\n", gresult);

  return 0;
}
