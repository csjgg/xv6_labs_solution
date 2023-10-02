#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

int find(char *dir, char *name) {
  int retnval = 0;
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  if ((fd = open(dir, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", dir);
    return -1;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", dir);
    close(fd);
    return -1;
  }
  if (st.type != T_DIR) {
    return 0;
  }
  if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof buf) {
    printf("find: path too long\n");
    return -1;
  }
  strcpy(buf, dir);
  p = buf + strlen(buf);
  *p++ = '/';
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    if(strcmp(de.name,".") ==0){
      continue;
    }
    if(strcmp(de.name,"..")==0){
      continue;
    }
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if (stat(buf, &st) < 0) {
      printf("find: cannot stat %s\n", buf);
      continue;
    }
    if(strcmp(de.name,name)==0){
      printf("%s\n",buf);
      retnval = 1;
    }
    if(st.type == T_DIR){
      int val = find(buf,name);
      if(retnval == 0){
        retnval = val;
      }
    }
  }
  return retnval;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("find: arguments wrong\n");
    exit(0);
  }
  char *dir = argv[1];
  char *name = argv[2];
  if (find(dir, name) == 0) {
    printf("find: (%s) no such file in %s\n", name, dir);
  }
  exit(0);
}
