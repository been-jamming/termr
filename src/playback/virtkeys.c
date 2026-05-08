#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <string.h>
#include <stdlib.h>
#include "virtkeys.h"

static int fd;

void init_virtkeys(void){
	struct uinput_setup usetup;

	fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_KEYBIT, KEY_LEFTCTRL);
	ioctl(fd, UI_SET_KEYBIT, KEY_LEFTCTRL);
	ioctl(fd, UI_SET_KEYBIT, KEY_KPPLUS);
	ioctl(fd, UI_SET_KEYBIT, KEY_KPMINUS);

	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = 0x0000;
	usetup.id.product = 0x0000;
	strcpy(usetup.name, "termr virtual keyboard");

	ioctl(fd, UI_DEV_SETUP, &usetup);
	ioctl(fd, UI_DEV_CREATE);
}

void deinit_virtkeys(void){
	ioctl(fd, UI_DEV_DESTROY);
	close(fd);
}

static void emit(int type, int code, int val){
	struct input_event ie;

	ie.type = type;
	ie.code = code;
	ie.value = val;

	ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;

	write(fd, &ie, sizeof(ie));
}

void zoom_in(void){
	emit(EV_KEY, KEY_LEFTCTRL, 1);
	emit(EV_SYN, SYN_REPORT, 0);
	emit(EV_KEY, KEY_KPPLUS, 1);
	emit(EV_SYN, SYN_REPORT, 0);
	emit(EV_KEY, KEY_KPPLUS, 0);
	emit(EV_SYN, SYN_REPORT, 0);
	emit(EV_KEY, KEY_LEFTCTRL, 0);
	emit(EV_SYN, SYN_REPORT, 0);
}

void zoom_out(void){
	emit(EV_KEY, KEY_LEFTCTRL, 1);
	emit(EV_SYN, SYN_REPORT, 0);
	emit(EV_KEY, KEY_KPMINUS, 1);
	emit(EV_SYN, SYN_REPORT, 0);
	emit(EV_KEY, KEY_KPMINUS, 0);
	emit(EV_SYN, SYN_REPORT, 0);
	emit(EV_KEY, KEY_LEFTCTRL, 0);
	emit(EV_SYN, SYN_REPORT, 0);
}

