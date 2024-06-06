#include <linux/module.h>

int my_module_init(void) {
	printk("Hello World\n");
	return 0;
}

void my_module_exit(void) {
	printk("Bye bye\n");
}

module_init(my_module_init);
module_exit(my_module_exit);
MODULE_LICENSE("GPL");
