#include <stdio.h>  

int main()  
{  
   int x, y;  

   printf("첫번째 입력 :: ");  
   scanf("%d", &x);  

   printf("두번째 입력 :: ");  
   scanf("%d", &y);  

   printf("두 입력 값 %d 와 %d의 합은 %d입니다", x, y, x + y);  

   return 0;  
}  
