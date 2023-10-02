#include "kernel/types.h"
#include "user/user.h"

int main(){
  int pipe1[2];
  int pipe2[2];
  int fd;
  pipe(pipe1);
  pipe(pipe2);
  fd = fork();
  if(fd < 0){
    write(1, "pingpong: fork wrong\n", sizeof("pingpong: fork wrong\n"));
    exit(0);
  }else if(fd == 0){
    char buf;
    // close two useless fd
    close(pipe1[1]);
    close(pipe2[0]);
    if(read(pipe1[0], &buf, 1) !=0){
      printf("%d: received ping\n", getpid());
    }
    close(pipe1[0]);
    write(pipe2[1],&buf,1);
    close(pipe2[1]);
    exit(0);
  }else if(fd > 0){
    close(pipe1[0]);
    close(pipe2[1]);
    char buf = 'a';
    write(pipe1[1],&buf, 1);
    close(pipe1[1]);
    if(read(pipe2[0], &buf, 1)!=0){
      printf("%d: received pong\n", getpid());
    }
    close(pipe2[0]);
    exit(0);
  }
  exit(0);
}