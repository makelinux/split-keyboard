#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>

u16 keys[][48] = {
	{
		KEY_ESC, KEY_PRINT, KEY_DELETE, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
		KEY_MACRO5, KEY_MACRO6, KEY_GRAVE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
		KEY_MACRO7, KEY_MACRO8, KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T,
		KEY_MACRO9, KEY_MACRO10, KEY_BACKSLASH, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G,
		KEY_MACRO11, KEY_MACRO12, KEY_LEFTSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B,
		KEY_RECORD + KEY_STOP, KEY_MACRO12,
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
	},
	{
		/* Additional keys that need to be registered but aren't mapped to physical keys */
		KEY_CAPSLOCK, KEY_RECORD, KEY_STOP
	}
};

static bool handle_caps_lock(struct input_dev *id, u16 key, bool pressed)
{
	static union {
		struct {
			u8 left:1;
			u8 right:1;
		};
		u8 both;
	} shift_state;

	if (key != KEY_LEFTSHIFT && key != KEY_RIGHTSHIFT)
		return false;

	u8 old_both = shift_state.both;

	if (key == KEY_LEFTSHIFT)
		shift_state.left = pressed;
	else
		shift_state.right = pressed;

	if (shift_state.both == 3 && old_both != 3) {
		input_report_key(id, KEY_CAPSLOCK, 1);
		input_report_key(id, KEY_CAPSLOCK, 0);
	}

	return true;
}

static int ptt(struct input_dev *id, s32 value)
{
	pr_devel("PTT %d\n", value);

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
		input_report_key(id, KEY_LEFTCTRL, 1);
		input_report_key(id, KEY_LEFTALT, 1);
		input_report_key(id, KEY_LEFTSHIFT, 1);
		input_report_key(id, f, 1);
	} else {
		input_report_key(id, f, 0);
		input_report_key(id, KEY_LEFTSHIFT, 0);
		input_report_key(id, KEY_LEFTALT, 0);
		input_report_key(id, KEY_LEFTCTRL, 0);
		input_report_key(id, KEY_MACRO_RECORD_START, 0);
		//input_report_key(id, KEY_MACRO_RECORD_STOP, 1);
		//input_report_key(id, KEY_MACRO_RECORD_STOP, 0);
	}
	return 1;
}

u16 keymap[2][265];

/* dm - dynamic macro:
 * first Ctrl+Alt+Enter to record
 * Enter to stop
 * Ctrl+Alt+Enter again to replay
 */

#define DM_SIZE 256

static struct {
	struct { u16 key; u8 val; } buf[DM_SIZE];
	int len;
	bool rec, eat_rel, pending;
	u8 mods;
} dm;

static bool dm_is_mod(u16 k)
{
	return k == KEY_LEFTCTRL || k == KEY_RIGHTCTRL ||
	       k == KEY_LEFTALT || k == KEY_RIGHTALT;
}

static int dm_event(struct input_dev *id, u16 k, s32 value)
{
	if (dm_is_mod(k))
		dm.mods = value ? dm.mods | (k == KEY_LEFTCTRL || k == KEY_RIGHTCTRL ? 1 : 2)
		               : dm.mods & ~(k == KEY_LEFTCTRL || k == KEY_RIGHTCTRL ? 1 : 2);

	if (k == KEY_ENTER && (dm.eat_rel || dm.pending)) {
		if (!value)
			dm.eat_rel = false;
		return 1;
	}
	pr_devel("key %d %d rec %d dm.mods %d\n", k, value, dm.rec, dm.mods);

	if (k == KEY_ENTER && value && (dm.mods & 3) == 3) {
		pr_devel("dm: %s %d\n", dm.len ? "pending" : "rec", dm.len);
		dm.eat_rel = true;
		if (dm.len)
			dm.pending = true;
		else
			dm.rec = true;
		return 1;
	}

	if (dm.rec && dm.len < DM_SIZE && !dm_is_mod(k)) {
		pr_devel("rec %d %d %d\n", dm.len, k, value);
		dm.buf[dm.len++] = (typeof(dm.buf[0])){k, value};
	}

	if (k == KEY_ENTER && !value && dm.rec) {
		pr_devel("dm: stop, %d keys\n", dm.len);
		dm.rec = false;
	}

	if (!dm.pending || dm.mods)
		return 0;

	input_report_key(id, k, value);
	input_sync(id);
	dm.pending = false;
	for (int i = 0; i < dm.len; i++) {
		pr_devel("replay %d %d %d\n", i, dm.buf[i].key, dm.buf[i].val);
		input_report_key(id, dm.buf[i].key, dm.buf[i].val);
		input_sync(id);
	}
	return 1;
}

#define hid_to_usb_dev(hid_dev) \
	to_usb_device(hid_dev->dev.parent->parent)

static int split_keyboard_event(struct hid_device *hid, struct hid_field *field,
                                struct hid_usage *u, __s32 value)
{
	struct input_dev *id = field->hidinput->input;
	struct usb_device *dev = hid_to_usb_dev(hid);
	bool right = dev->devpath[strlen(dev->devpath) - 1] & 1;
	u16 k = keymap[right][u->code];
	static bool drop;

	// drop redundant events
	if (u->code == 70) {
		if (!value)
			drop ^= true;
		pr_devel("%d %d %d\n", u->code, value, drop);
		if (drop)
			return 0;
	}

	if (k)
		pr_devel("%d %d -> %d\n", u->code, value, k);

	if (k && dm_event(id, k, value))
		return 1;

	handle_caps_lock(id, k, value);

	if (k == KEY_RECORD + KEY_STOP)
		return ptt(id, value);
	if (k >= KEY_MACRO1 &&
		k <= KEY_MACRO12) {
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

static int dev_num(const char *name)
{
	int n;
	return sscanf(name, "input%d", &n) == 1 ? n : -1;
}

static int input_configured(struct hid_device *hid,
		struct hid_input *hidinput)
{
	struct input_dev *id = hidinput->input;
	struct usb_device *dev = hid_to_usb_dev(hid);
	bool right = dev->devpath[strlen(dev->devpath) - 1] & 1;
	printk("%s %s %s %d %d\n", __func__, dev_name(&id->dev), dev->devpath, right,
	       dev_num(dev_name(&id->dev)));

	for (int i = 0; i < ARRAY_SIZE(koolertron_48_map); i++) {
		keymap[right][koolertron_48_map[i]] = keys[right][i];
		input_set_capability(id, EV_KEY, keys[right][i]);
	}
	id->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_LED) | BIT_MASK(EV_REP);
	id->ledbit[0] = BIT_MASK(LED_NUML) | BIT_MASK(LED_CAPSL) |
		BIT_MASK(LED_SCROLLL) | BIT_MASK(LED_COMPOSE);

	for (int i = 0; keys[2][i] && i < 48; i++)
		input_set_capability(id, EV_KEY, keys[2][i]);

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
