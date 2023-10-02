#include "kernel/types.h"
#include "user/user.h"
#include <stddef.h>


int exenext(int readfd){
  int num;
  if(read(readfd,&num, 4)==0){
    return -1;
  }
  printf("prime %d\n",num);
  int get;
  int mpipe[2];
  pipe(mpipe);
  while(read(readfd,&get,4)){
    if(get%num){
      write(mpipe[1],&get,4);
    }
  }
  close(mpipe[1]);
  close(readfd);
  return mpipe[0];
}

int main(){
  int mpipe[2];
  pipe(mpipe);
  for(int i = 2; i<35;i++){
    write(mpipe[1], &i, 4);
  }
  close(mpipe[1]);
  int tag = exenext(mpipe[0]);
  while(tag!=-1){
    int fd = fork();
    if(fd < 0){
      printf("primes: fork error\n");
    }else if(fd==0){
      tag = exenext(tag);
    }else if(fd > 0){
      wait(NULL);
      break;
    }
  }
  exit(0);
}