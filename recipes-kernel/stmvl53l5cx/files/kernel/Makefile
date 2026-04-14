obj-m += stmvl53l5cx.o
stmvl53l5cx-objs += stmvl53l5cx_module.o stmvl53l5cx_i2c.o

SRC := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean
