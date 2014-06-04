KERNEL_FILES = kernel.c vga.c print.c pic.c thread.c palloc_np.c string.c palloc.c rand.c saper.c
KERNEL_HEADERS = vga.h io.h print.h pic.h main_debug.h thread.h multiboot.h palloc_np.h string.h palloc.h rand.h

kernel: $(KERNEL_FILES) kernel.lds $(KERNEL_HEADERS)
	gcc -Wall -m32 -ffreestanding -nostdlib $(KERNEL_FILES) -o kernel -Wl,-T,kernel.lds

install: kernel
	sudo mount fd.img fd
	sudo cp kernel fd/
	sudo umount fd

make_fd:
	dd if=/dev/zero of=fd.img bs=512 count=2880
	mkfs.vfat fd.img

run: kernel
	qemu-system-i386 -kernel kernel

clean:
	rm -f kernel

.PHONY: run make_fd install clean
