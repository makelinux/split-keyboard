#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>

u16 keys[2][48] = {
	{
		KEY_ESC, KEY_PRINT, KEY_DELETE, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
		KEY_MACRO5, KEY_MACRO6, KEY_GRAVE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
		KEY_MACRO7, KEY_MACRO8, KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T,
		KEY_CAPSLOCK, KEY_MACRO9, KEY_BACKSLASH, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G,
		KEY_MACRO10, KEY_MACRO11, KEY_LEFTSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B,
		KEY_MACRO12, KEY_STOP,
			KEY_LEFTCTRL, KEY_LEFTMETA, KEY_COMPOSE, KEY_LEFTALT, KEY_SPACE, KEY_SPACE,
		// KEY_STOP -> Cancel
		// KEY_STOPCD -> Audio stop, but is not invoked
	},
	{
		KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_INSERT,
		KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE,
		KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_PAGEUP,
		KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_ENTER, KEY_PAGEDOWN,
		KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_HOME, KEY_UP, KEY_END,
		KEY_SPACE, KEY_RIGHTALT,
			KEY_COMPOSE, KEY_RIGHTCTRL, KEY_RIGHTSHIFT, KEY_LEFT, KEY_DOWN, KEY_RIGHT,
	}
};

static int ptt(struct input_dev *id, s32 value)
{
	if (value) {
		input_report_key(id, KEY_RECORD, 1);
		input_report_key(id, KEY_RECORD, 0);
	} else {
		input_report_key(id, KEY_STOP, 1);
		input_report_key(id, KEY_STOP, 0);
	}
	return 1;
}

static int macro(struct input_dev *id, int f, s32 value)
{
	if (value) {
		input_report_key(id, KEY_LEFTALT, 1);
		input_report_key(id, KEY_LEFTCTRL, 1);
		input_report_key(id, KEY_LEFTSHIFT, 1);
		input_report_key(id, f, 1);
	} else {
		input_report_key(id, f, 0);
		input_report_key(id, KEY_LEFTSHIFT, 0);
		input_report_key(id, KEY_LEFTCTRL, 0);
		input_report_key(id, KEY_LEFTALT, 0);
		input_report_key(id, KEY_MACRO_RECORD_START, 0);
		//input_report_key(id, KEY_MACRO_RECORD_STOP, 1);
		//input_report_key(id, KEY_MACRO_RECORD_STOP, 0);
	}
	return 1;
}

u16 keymap[2][265];

#define hid_to_usb_dev(hid_dev) \
	to_usb_device(hid_dev->dev.parent->parent)

static int split_keyboard_event(struct hid_device *hid, struct hid_field *field,
                                struct hid_usage *u, __s32 value)
{
	struct input_dev *id = field->hidinput->input;
	struct usb_device *dev = hid_to_usb_dev(hid);
	bool right = dev->devpath[strlen(dev->devpath) - 1] & 1;
	u16 k = keymap[right][u->code];
	if (k)
		pr_devel("%d %d -> %d\n", u->code, value, k);
	if (k >= KEY_MACRO1 &&
		k <= KEY_MACRO12) {
		if (k == KEY_MACRO12)
			return ptt(id, value);
		int f = k - KEY_MACRO1 + KEY_F1;
		return macro(id, f, value);
	}

	input_report_key(id, k, value);

	return 1;
}

#define USB_VENDOR_ID_CYPRESS          0x04b4

int koolertron_48_map[] = {
	/*KEY_A*/30, 48, 46, 32, 18, 33, 34, 70,
	/*KEY_H*/35, 23, 36, 37, 69, 98, 55, 74,
	38, 50, 49, 24, 71, 72, 73, 78,
	25, 16, 19, 31, 75, 76, 77, 96,
	/* KEY_T */20, 22, 47, 17, 79, 80, 81, 82,
	/* KEY_X */45, 21, 44, 83, /*KEY_LEFT*/105, 106, 103, 108,
};

static int input_configured(struct hid_device *hid,
		struct hid_input *hidinput)
{
	struct input_dev *id = hidinput->input;
	struct usb_device *dev = hid_to_usb_dev(hid);
	bool right = dev->devpath[strlen(dev->devpath) - 1] & 1;
	printk("%s %s %d\n", __func__, dev->devpath, right);

	for (int i = 0; i < ARRAY_SIZE(koolertron_48_map); i++) {
		keymap[right][koolertron_48_map[i]] = keys[right][i];
		input_set_capability(id, EV_KEY, keys[right][i]);
	}
	id->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_LED) | BIT_MASK(EV_REP);
	id->ledbit[0] = BIT_MASK(LED_NUML) | BIT_MASK(LED_CAPSL) |
		BIT_MASK(LED_SCROLLL) | BIT_MASK(LED_COMPOSE);

	return 0;
}


static const struct hid_device_id split_keyboard_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_CYPRESS, 0x0818) },
	{ }
};
MODULE_DEVICE_TABLE(hid, split_keyboard_devices);

static int hid_generic_probe(struct hid_device *hdev,
                             const struct hid_device_id *id)
{
	int ret;

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	return hid_hw_start(hdev, HID_CONNECT_DEFAULT);
}

static struct hid_driver split_keyboard_driver = {
	.name = "split-keyboard",
	.probe = hid_generic_probe,
	.id_table = split_keyboard_devices,
	.event = split_keyboard_event,
	.input_configured = input_configured,
};
module_hid_driver(split_keyboard_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Split keyboard");
