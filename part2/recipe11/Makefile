obj-m += recipe11.o
KVERSION = $(shell uname -r)

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
deploy:
	sshpass -praspberry scp -o StrictHostKeyChecking=no *.ko pi@192.168.1.100:/home/pi/module/
