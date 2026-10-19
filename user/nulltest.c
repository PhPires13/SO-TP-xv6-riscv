// Tests for null-pointer dereference protection
// Page 0 must be unmapped: any access to it kills the process,
// system calls given pointers into it fail, and no memory leaks

#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/riscv.h"
#include "user/user.h"

#define NROUNDS 10

int failures = 0;

void check(int ok, char *what) {
  printf("%s: %s\n", ok ? "ok" : "FAILED", what);
  if (!ok)
    failures++;
}

// faulting actions; each should get the process killed
void readnull(void) {
  printf("read *0 = %d\n", *(volatile int *)0);
}

void writenull(void) {
  *(volatile int *)0 = 42;
}

void callnull(void) {
  void (*volatile f)(void) = 0;
  f();
}

void readlastnullbyte(void) {
  printf("read *0xfff = %d\n", *(volatile char *)(PGSIZE - 1));
}

// run f in a child; return 1 if the kernel killed it (exit status -1)
int killedby(void (*f)(void)) {
  int status;
  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    f();
    exit(0); // reached only if the access did not fault
  }
  wait(&status);
  return status == -1;
}

// number of free physical pages, measured in a child so that
// the parent's memory is left untouched
int countfree(void) {
  int fds[2], n = 0;

  if (pipe(fds) < 0) {
    printf("pipe failed\n");
    exit(1);
  }
  if (fork() == 0) {
    close(fds[0]);
    while (sbrk(PGSIZE) != SBRK_ERROR)
      n++;
    write(fds[1], &n, sizeof(n));
    exit(0);
  }
  close(fds[1]);
  read(fds[0], &n, sizeof(n));
  close(fds[0]);
  wait(0);
  return n;
}

int main(void) {
  int fd, free0;
  char buf[8];

  printf("nulltest starting\n");

  // invalid accesses to page 0 are flagged and the process is killed
  check(killedby(readnull), "read from address 0 kills the process");
  check(killedby(writenull), "write to address 0 kills the process");
  check(killedby(callnull), "jump to address 0 kills the process");
  check(killedby(readlastnullbyte), "read from address 0xfff kills the process");

  // the program itself now starts at USERBASE (0x1000)
  check((uint64)main >= PGSIZE, "program text is above the null page");
  check(*(volatile char *)PGSIZE == *(char *)PGSIZE, "address 0x1000 is readable");

  // the kernel refuses null pointers passed to system calls
  fd = open("nulltest.tmp", O_CREATE | O_RDWR);
  check(write(fd, (char *)0, sizeof(buf)) == -1, "write() from address 0 fails");
  write(fd, "data", 4);
  close(fd);
  fd = open("nulltest.tmp", O_RDONLY);
  check(read(fd, (char *)0, sizeof(buf)) == -1, "read() into address 0 fails");
  close(fd);
  unlink("nulltest.tmp");
  check(open((char *)0, O_RDONLY) == -1, "open() of path at address 0 fails");
  check(pipe((int *)0) == -1, "pipe() into address 0 fails");

  // the heap cannot shrink into the null page
  check(sbrk(-(int)(uint64)sbrk(0)) == SBRK_ERROR, "sbrk cannot shrink into page 0");

  // killing faulting processes does not leak memory
  free0 = countfree();
  for (int i = 0; i < NROUNDS; i++) {
    killedby(readnull);
    killedby(writenull);
    killedby(callnull);
  }
  check(countfree() == free0, "no pages leaked by killed processes");

  if (failures) {
    printf("nulltest: %d FAILED\n", failures);
    exit(1);
  }
  printf("nulltest: ALL TESTS PASSED\n");
  exit(0);
}
