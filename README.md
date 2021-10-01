# Char device task project 

1. Cross-compile module

*make -j$(nproc) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules*

- This operation should be performed with preinstalled rapbpios kernel build tools.
- To get and build your own kernel follow the [link](https://www.raspberrypi.org/documentation/computers/linux_kernel.html)
- Kernel sources should be downloaded and build. After your kernel will be stored in linux/ directory.

- Module directory should be *linux/drivers/char_led/*

- Before building add line to Kconfig file in *linux/drivers* 

*source "drivers/char_led/Kconfig"*
- Before building add line to Makefile in *linux/drivers*

*obj-$(CONFIG_CHAR_LED)  += char_led/*

2. Copy char_led.ko file to the raspberry pi device using ssh.

Example of such copy:

*scp /home/strngr/linux/drivers/char_led/char_led.ko pi@192.168.0.101:/home/pi/*

3. Perform: sudo insmod char_led.ko

Note: If module can not be installed because of it's build on a newer kernel version. (It could be checked with *modinfo module_name* command). The kernel could be updated using *sudo rpi_update*. After the job is done use *sudo reboot*

4. When the module is installed it could be checked using lsmod command

5. Load module using the load.sh script
from current directory:

*sudo chmod +x ./load.sh*

*sudo ./load.sh*

6. Next module should be seen in /dev/ directory. Check it with 

	ls /dev/* | grep 'LED_CTRL'

7. Check the module is working with commands
 - For red led:

    **
    
    *sudo bash ./test.sh 0 10 200*

 - For blue led:

   *sudo bash ./test.sh 1 10 200*

