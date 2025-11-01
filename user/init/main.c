void _start(void) {
  const char* msg = "Hello from user\n";
  asm volatile (
    "mov $1, %%eax\n"
    "mov %0, %%ebx\n"
    "int $0x80\n"
    "mov $0, %%eax\n"
    "xor %%ebx, %%ebx\n"
    "int $0x80\n"
    :: "r"(msg) : "eax", "ebx"
  );
}
