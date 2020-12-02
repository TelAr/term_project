#--------------Makefile---------------------#

obj-m := linked_list.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default :
	$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) modules
claen :
	$(MAKE) -C $(KERNEL_DIR) M=$(shell pwd) claen
