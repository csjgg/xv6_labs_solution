#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
  if(argc!=2){
    write(1, "sleep: argument wrong\n",strlen("sleep: argument not enough\n"));
    exit(0);
  }
  int time = atoi(argv[1]);
  if(time == 0){
    write(1, "sleep: argument wrong\n",strlen("sleep: argument not enough\n"));
    exit(0);
  }else{
    sleep(time);
    exit(0);
  }
}