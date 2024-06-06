obj-m := scull.o
all:
	make -C /lib/modules/6.9.1-arch1-Adashima-T2-1-t2/build M=$(PWD) modules
clean:
	rm -rf *.o *.ko *.mod.c .an* .lab* .tmp_versions Module.symvers Module.markers modules.order .*.cmd *.mod
