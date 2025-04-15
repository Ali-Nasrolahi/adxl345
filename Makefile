obj-m += adxl.o
adxl-objs := adxldev.o adxl-core.o adxl-fops.o adxl-sysfs.o

#CFLAGS_EXTRA += -DDEBUG
#KERNEL_SRC = $(KERNELDIR)
KERNEL_SRC = /lib/modules/$(shell uname -r)/source

all: app
	make -C $(KERNEL_SRC) M=$(shell pwd) modules

clean:
	make -C $(KERNEL_SRC) M=$(shell pwd) clean
	rm app || true

format:
	clang-format -i -style=file *.c *.h

load: unload all
	sudo insmod adxl.ko

unload:
	sudo rmmod adxl || true

app: app.c
	$(CC) app.c -o app
