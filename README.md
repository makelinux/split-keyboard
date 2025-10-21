# Split Keyboard Driver

Linux kernel driver for programmable split keyboard based
on two Koolertron 48-key keyboards.

## Device Information
- **Vendor ID**: 0x04b4 (Cypress)
- **Product ID**: 0x0818
- **Keys per half**: 48

## Keyboard Layouts

The driver remaps the OEM keys to provide the following layouts:

#### Left keyboard

|          |        |        |       |     |      |     |     |
|----------|--------|--------|-------|-----|------|-----|-----|
| ESC      | Print  | Delete | F1    | F2  | F3   | F4  | F5  |
| Macro5   | Macro6 | `      | 1     | 2   | 3    | 4   | 5   |
| Macro7   | Macro8 | Tab    | Q     | W   | E    | R   | T   |
| CapsLock | Macro6 | \      | A     | S   | D    | F   | G   |
| Macro7   | Record | LShift | Z     | X   | C    | V   | B   |
| Rec/Stop | Stop   | LCtrl  | LMeta | Cmp | LAlt | Spc | Spc |

#### Right keyboard

|     |        |     |       |         |      |       |        |
|-----|--------|-----|-------|---------|------|-------|--------|
| F6  | F7     | F8  | F9    | F10     | F11  | F12   | Insert |
| 6   | 7      | 8   | 9     | 0       | -    | =     | BS     |
| Y   | U      | I   | O     | P       | [    | ]     | PgUp   |
| H   | J      | K   | L     | ;       | '    | Enter | PgDn   |
| N   | M      | ,   | .     | /       | Home | ↑     | End    |
| Spc | RAlt   | Cmp | RCtrl | RShift  | ←    | ↓     | →      |

## Special keys

### Macro keys
- **Macro1-Macro8**: Trigger Alt+Ctrl+Shift+F[n] combinations
- **Macro9**: Push-to-Talk (PTT) function
  - Press: sends RECORD key
  - Release: sends STOP key

### Function keys
- **Record**: Used for PTT press event
- **Stop**: Used for PTT release event
- **Compose**: Available on both halves

## Notes
- The driver automatically detects left/right keyboard based on USB device path
- Each half maintains its own keymap
- All 48 keys per half are fully programmable through the keymap arrays

## OEM product
[Koolertron Single-Handed Programmable Mechanical Keyboard](https://www.koolertron.com/koolertron-single-handed-programmable-mechanical-keyboard-pbt-blank-no-print-and-color-keycaps-for-gamers-designers-editors-all-48-programmable-keys-with-oem-gateron-red-switch-8-macro-keys.html)

### OEM physical layout

The Koolertron keyboard sends the following keys (based on USB HID codes):

|   |   |   |     |      |       |     |         |
|---|---|---|-----|------|-------|-----|---------|
| A | B | C | D   | E    | F     | G   | ScrLock |
| H | I | J | K   | NumLk| KP/   | KP* | KP-     |
| L | M | N | O   | KP7  | KP8   | KP9 | KP+     |
| P | Q | R | S   | KP4  | KP5   | KP6 | KPEnt   |
| T | U | V | W   | KP1  | KP2   | KP3 | KP0     |
| X | Y | Z | KP. | Left | Right | Up  | Down    |

These keys are remapped by the driver to create the custom split keyboard.
