#include <stdint.h>
#include <stddef.h>
#include "idt.h"
#include "io.h"
#include "apic.h"
#include "malloc.h"
#include "msr.h"
#include "stdio.h"
#include "utils.h"

uint8_t stack[0x1000];

asm(
	".global _start\n"
	"_start:\n"
	"movabsq $stack+0x1000, %rsp\n"
	"call main\n"
	"cli\n"
	"hlt\n"
   );

asm(
	"user_code:\n"
	".incbin \"user.bin\"\n"
	"user_code_end:\n"
);

void div0() {
	printf("Division by 0!\n");
	printf("System halted!\n");
	asm ("cli; hlt");
}

asm(
	".global syscall_asm\n"
	"syscall_asm:\n"
	"pushq %rax\n"
	"pushq %rcx\n"
	"pushq %rdx\n"
	"pushq %rbx\n"
	"pushq %rbp\n"
	"pushq %rsi\n"
	"pushq %rdi\n"
	"pushq %r8\n"
	"pushq %r9\n"
	"pushq %r10\n"
	"pushq %r11\n"
	"pushq %r12\n"
	"pushq %r13\n"
	"pushq %r14\n"
	"pushq %r15\n"
	"movq %rsp, %rdi\n"
	"call syscall\n"
	"popq %r15\n"
	"popq %r14\n"
	"popq %r13\n"
	"popq %r12\n"
	"popq %r11\n"
	"popq %r10\n"
	"popq %r9\n"
	"popq %r8\n"
	"popq %rdi\n"
	"popq %rsi\n"
	"popq %rbp\n"
	"popq %rbx\n"
	"popq %rdx\n"
	"popq %rcx\n"
	"popq %rax\n"
	"iretq\n"
);

asm(
	".global syscall_syscall_asm\n"
	"syscall_syscall_asm:\n"
	"movabsq %rax, stack+0xff0\n"
	"movq %rsp, %rax\n"
	"movabsq %rax, stack+0xff8\n"
	"movabsq $stack+0xff0, %rsp\n"
	"pushq %rcx\n"
	"pushq %rdx\n"
	"pushq %rbx\n"
	"pushq %rbp\n"
	"pushq %rsi\n"
	"pushq %rdi\n"
	"pushq %r8\n"
	"pushq %r9\n"
	"pushq %r10\n"
	"pushq %r11\n"
	"pushq %r12\n"
	"pushq %r13\n"
	"pushq %r14\n"
	"pushq %r15\n"
	"movq %rsp, %rdi\n"
	"call syscall\n"
	"popq %r15\n"
	"popq %r14\n"
	"popq %r13\n"
	"popq %r12\n"
	"popq %r11\n"
	"popq %r10\n"
	"popq %r9\n"
	"popq %r8\n"
	"popq %rdi\n"
	"popq %rsi\n"
	"popq %rbp\n"
	"popq %rbx\n"
	"popq %rdx\n"
	"popq %rcx\n"
	"popq %rax\n"
	"popq %rsp\n"
	"sysretq\n"
);

asm(
	".global irq_asm\n"
	"irq1_asm:\n"
	"pushq %rax\n"
	"pushq %rcx\n"
	"pushq %rdx\n"
	"pushq %rsi\n"
	"pushq %rdi\n"
	"pushq %r8\n"
	"pushq %r9\n"
	"pushq %r10\n"
	"pushq %r11\n"
	"call irq1\n"
	"popq %r11\n"
	"popq %r10\n"
	"popq %r9\n"
	"popq %r8\n"
	"popq %rdi\n"
	"popq %rsi\n"
	"popq %rdx\n"
	"popq %rcx\n"
	"popq %rax\n"
	"iretq\n"
);

struct regs {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rax;
};

void syscall(struct regs *regs) {
	switch (regs->rax) {
		case 0: {
			char s[2] = { regs->rdx, 0 };
			printf(s);
			break;
		}
		default:
			printf("Unknown syscall\n");
			break;
	}
}

void irq1() {
	printf("IRQ1\n");
	unsigned char c = inb(0x60);
	char buf[] = "Byte 0x?? received by IRQ\n";
	buf[8] = "0123456789abcdef"[c & 0xf];
	buf[7] = "0123456789abcdef"[c >> 4];
	printf(buf);
	write_lapic(0xb0, 0);
}


#define PHYS_BASE 0xffff800000000000ull

void map_page_user(uint64_t virt, uint64_t phys) {
	uint64_t i1 = virt >> 12 & 0x1ff;
	uint64_t i2 = virt >> 21 & 0x1ff;
	uint64_t i3 = virt >> 30 & 0x1ff;
	uint64_t i4 = virt >> 39 & 0x1ff;
	uint64_t p4;
	uint64_t p3;
	uint64_t p2;
	uint64_t p1;
	asm (
		"movq %%cr3, %0\n"
		: "=r"(p4)
		:
	);
	uint64_t *pt4 = (void *)(PHYS_BASE + p4);
	if (!(pt4[i4] & 1)) {
		void *npt = alloc_pages(1);
		pt4[i4] = ((uint64_t)npt - PHYS_BASE) | 7;
	}
	p3 = pt4[i4] & ~0xfff;
	uint64_t *pt3 = (void *)(PHYS_BASE + p3);
	if (!(pt3[i3] & 1)) {
		void *npt = alloc_pages(1);
		pt3[i3] = ((uint64_t)npt - PHYS_BASE) | 7;
	}
	p2 = pt3[i3] & ~0xfff;
	uint64_t *pt2 = (void *)(PHYS_BASE + p2);
	if (!(pt2[i2] & 1)) {
		void *npt = alloc_pages(1);
		pt2[i2] = ((uint64_t)npt - PHYS_BASE) | 7;
	}
	p1 = pt2[i2] & ~0xfff;
	uint64_t *pt1 = (void *)(PHYS_BASE + p1);
	pt1[i1] = phys | 7;
}

void *memcpy(void *dst, const void *src, size_t sz) {
	char *cdst = dst;
	const char *csrc = src;
	while (sz--)
		*cdst++ = *csrc++;
	return dst;
}

asm(
	"enter_userspace:\n"
	"pushq $0x33\n"
	"pushq $0\n"
	"pushq $0x200\n"
	"pushq $0x3b\n"
	"pushq %rdi\n"
	"iretq\n"
);

int main() {
	init_gdt();
	init_idt();
	set_idt_entry(0, (uint64_t)&div0, true);
	printf("Hello, 64-bit world!\n");
	outb(0x20, 0x11);
	outb(0x21, 0x20);
	outb(0x21, 0x04);
	outb(0x21, 0x01);
	outb(0xa0, 0x11);
	outb(0xa1, 0x28);
	outb(0xa1, 0x02);
	outb(0xa1, 0x01);
	outb(0x21, 0xff);
	outb(0xa1, 0xff);
	write_lapic(0xf0, 0x1ff);
	extern char irq1_asm;
	extern char syscall_asm;
	set_idt_entry(0x31, (uint64_t)&irq1_asm, false);
	write_ioapic(0x12, 0x00000031);
	write_ioapic(0x13, 0x00000000);
	set_idt_entry(0xfe, (uint64_t)&syscall_asm, true);

	*(uint64_t *)0x8e000 = 0;
	asm (
		"movq %%cr3, %%rax\n"
		"movq %%rax, %%cr3\n"
		:
		:
		:
		"rax"
	);
	void *user_page = alloc_pages(1);
	map_page_user(0x123456789000, (uint64_t)user_page - PHYS_BASE);
	extern char user_code[];
	extern char user_code_end[];
	extern char syscall_syscall_asm[];
	memcpy(user_page, user_code, user_code_end - user_code);
	extern void enter_userspace(uint64_t);
	wrmsr(0xc0000080, rdmsr(0xc0000080) | 1);
	wrmsr(0xc0000081, 0x002b001800000000);
	wrmsr(0xc0000082, (uint64_t)&syscall_syscall_asm);
	wrmsr(0xc0000083, (uint64_t)&syscall_syscall_asm);
	wrmsr(0xc0000084, 0xffffffff);
	set_tss_rsp((uint64_t)stack + sizeof stack);
	enter_userspace(0x123456789000);

	asm("sti");
	while(1) {
		asm("hlt");
	}
#if 0
	volatile int a = 1;
	volatile int b = 0;
	a /= b;
#endif
	while (1) {
		uint8_t c = getc();
		char buf[] = "Byte 0x?? received\n";
		buf[8] = "0123456789abcdef"[c & 0xf];
		buf[7] = "0123456789abcdef"[c >> 4];
		printf(buf);
	}
	return 0;
}
