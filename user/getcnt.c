#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int num, cnt;

  if (argc != 2) {
    fprintf(2, "usage: getcnt syscall_number\n");
    exit(1);
  }

  num = atoi(argv[1]); // atoi("abc") is 0, which the kernel rejects below
  cnt = getcnt(num);
  if (cnt < 0) {
    fprintf(2, "getcnt: %s is not a valid syscall number\n", argv[1]);
    exit(1);
  }

  printf("syscall %d has been called %d times\n", num, cnt);
  exit(0);
}
