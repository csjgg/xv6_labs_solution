#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"
#include <stddef.h>

void execute_cmd(char *argv[]) {
  int fd = fork();
  if (fd < 0) {
    printf("xargs: fork error\n");
  } else if (fd == 0) {
    exec(argv[0], argv);
    exit(0);
  } else if (fd > 0) {
    wait(NULL);
  }
}

void free_cmd(char *argc[], int argv) {
  for (int i = argv; argc[i] != NULL; i++) {
    if (argc[i] != NULL) {
      free(argc[i]);
    }
  }
}

int main(int argc, char *argv[]) {
  char p;
  char buf[512];
  char *newarg[MAXARG];
  int argsize = 0;
  int i;
  for (i = 0; i < argc - 1; i++) {
    newarg[i] = argv[i + 1];
  }
  while (read(0, &p, 1) != 0) {
    switch (p) {
    case ' ':
      if (argsize == 0) {
        break;
      }
      buf[argsize] = '\0';
      newarg[i] = (char *)malloc(argsize);
      strcpy(newarg[i], buf);
      i++;
      argsize = 0;
      break;
    case '\n':
      if (argsize != 0) {
        buf[argsize++] = '\0';
        newarg[i] = (char *)malloc(argsize);
        argsize = 0;
        strcpy(newarg[i], buf);
        i++;
      }
      newarg[i] = NULL;
      i = argc - 1;
      execute_cmd(newarg);
      free_cmd(newarg, argc - 1);
      break;
    default:
      buf[argsize++] = p;
      break;
    }
  }
  if (i != argc - 1 || argsize != 0) {
    if (argsize != 0) {
      buf[argsize++] = '\0';
      newarg[i] = (char *)malloc(argsize);
      strcpy(newarg[i], buf);
      i++;
      argsize = 0;
    }
    newarg[i] = NULL;
    execute_cmd(newarg);
    free_cmd(newarg, argc - 1);
  }
  exit(0);
}