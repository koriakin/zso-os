#include "gdt.h"
#include "io.h"
#include "mb.h"
#include "page.h"
#include "panic.h"
#include "pic.h"
#include "pmalloc.h"
#include "printf.h"

void self_test() {
	asm volatile(
		"int $0x20\n" // Hello world!
		:::
	);

	// printf tests
	printf("123 == %u\n", 123);
	printf("0x123def == 0x%x\n", 0x123def);
	printf("'x' == '%c'\n", 'x');
	printf("\"asdf%%123\" == \"%s\"\n", "asdf%123");
}

_Noreturn void panic(const char *arg) {
	printf(arg);
	while (1) {
		asm volatile("cli\nhlt":::);
	}
}

void sys_hello() {
	printf("Hello, world\n");
}

void div_zero() {
	printf("Division by 0 - system halted\n");
	asm volatile("cli\nhlt");
}

void err_gp() {
	printf("General protection - system halted\n");
	asm volatile("cli\nhlt");
}

void err_page() {
	uint32_t cr2;
	asm volatile (
		"movl %%cr2, %0\n":
		"=r"(cr2):
		:
	);
	printf("Page fault (%x) - system halted\n", cr2);
	asm volatile("cli\nhlt");
}

void irq1() {
	uint8_t b = inb(0x60);
	printf("KEY %x\n", b);
	outb(0x20, 0x20);
}

extern char asm_sys_hello[];
extern char asm_div_zero[];
extern char asm_err_gp[];
extern char asm_err_page[];
extern char asm_irq1[];

void main(struct mb_header *mbhdr) {
	init_gdt();
	init_pmalloc(mbhdr);
	init_paging();
	set_idt(0x20, (uint32_t)asm_sys_hello, 0, 3);
	set_idt(0x00, (uint32_t)asm_div_zero, 0, 0);
	set_idt(0x0d, (uint32_t)asm_err_gp, 0, 0);
	set_idt(0x0e, (uint32_t)asm_err_page, 0, 0);
	init_pic();
	set_idt(0xf1, (uint32_t)asm_irq1, 1, 0);
	outb(0x21, 0xfd);

	self_test();


	asm volatile(
		"pushl $0x00\n"
		"pushl $0x00\n"
		"pushl $0x00\n"
		"pushl $0x00\n"
		"popl %%ds\n"
		"popl %%es\n"
		"popl %%fs\n"
		"popl %%gs\n"
		"pushl $0x2b\n"
		"pushl $user_stack+4096\n"
		"pushl $0x0002\n"
		"pushl $0x23\n"
		"pushl $user_main\n"
		"iretl\n"
		:::
	);
}
