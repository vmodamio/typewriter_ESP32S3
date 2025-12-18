/* Input event codes for the typewriter V1 
 * Note: that now I can just use a keycode[] array of 64, without layouts,
 * as the microcontroller for the keyboard and the typewriter are the same thing,
 * and the keyboard layouts can be handled in a similar way to the keymapping and 
 * language layout (deadkeys, modifiers, etc).
 * Note also: that this array is indexed using the wiring from typewriter V1 */


/* The idea here is to translate a KEY_CODE using the keyboard layouts into a KEY_SYM 
 * wether KEY_SYM can be a printable character or a ansi escape, or whatever... even 
 * program functions for bluethooth or filesystem accessing etc... */


/* Keyboard: The keyboard switches status is hold in uint8_t KEYS[64], 64 contiguous bytes. Each key 8bits used for the 
 * debouncing/denoising with shift registers.
 * Every time the status of a key is changed, a key event is put in a queue KEY_EVENTS. This is a 1 byte per event
 * representing a potentially 128-keys keyboard + 1 bit representing key UP/DOWN. For the 128 keys, only those with 
 * printable (or potentially printable via mods or deadkeys) are represented within the first 64 keys. The other 64 
 * dedicated to system keys and ANSI-scaped keys like Enter, Tab, Backspace, ESC, etc.
 * Note: NO, this difference needs to occur after the mapping, because of some keys like Arrow keys and many others.
 *
 * KEY_EVENT:   MSB [ 7 .. 1 | KEY_DOWN ] LSB 
 *
 * Example: Key A is pressed (code a = 30)   KEY_EVENT = (a << 1) + 1 = 61   
 *          Key Left Shift is depressed (code = 42) KEY_EVENT = (42 << 1) + 0 = 84 
 *            Note: here left and rigth shift can send the same code, even though two instances of shift down could happen
 *                  from the physically different keys.
 *
 * A key event popped from the queue is processed with a keymap. The keymap translates the key_event depending on the language
 * layout, modifiers, keyboard_layers, and deadkeys. First, a byte holding the status of the key-modifiers is 
 * updated. This holds { CapsLock , Shift, Ctrl, Alt, Win, Func, AltGr }.  
 */

/* Keyboard: reports with interrupts changes in the status of the keyboard switches (key_events)
 *           because we have <64, we encode the events as one byte, using the first bit for press-release statutus
 *           and the next 7bit (up to 128 keys) for the switch.
 *           Events with multiple bytes represent several keys pressed and/or depressed in the same scan, even though
 *           this is very unlikely.
 *           After processing the event, the keyboard status is updated (64 bits, 8 contiguous bytes called 'KBD_ROWS' for 
 *           example), and some common modifiers (Lshift,Rshift,Ctrl,AltGr, etc) are also updated in an extra byte 'KBD_MODS'
 *           for masking later. Maybe first bit of KBD_MODS use it for 'keyboard-modified' flag.
 *           Note: Now we dont need to hold the status of the keyboard! But the debouncing algorithm using the 8bit shift 
 *           register I still need it. So, instead of KBD_ROWS being 1 byte per row, I will need one byte per key 
 *           uint8_t KEYS[64] (actually a struct), each one processing the status of the KEY.
 *
 *           Now, map all the keys with printable (or potentially printable character via mods or deadkeys)
 *           With a keyboard status changed (keyboard-modified flag), some special keys (system keys) will already trigger
 *           some processes and flip back the flag, (like bluethooth or things like that, or editor special keymaps).
 *           The second bit of KBD_MODS tells wether the key needs to be processed as a character with further KEYMAP, or 
 *           is a system-KEY (go to menus, change language layout, etc)
 *
 *           KBD MODS register:
 *           KBD_MODS: MSB [ CAPS_LOCK | Shift | ALTGr | FUNC | ALT | CTRL | SYS | keyboard_modified_flag ] LSB
 *           
 *           With the flag still on, a KEYMAP is applied, depending on the keyboard layout loaded (ES, US, NO...), 
 *           processing the modifiers and another byte 'DEAD_KEYS', to finally produce a Character or instruction.
 *
 *           Debate: here, as the whole thing is the editor, instead of mapping characters to the editor normal mode actions,
 *           I could map it here straigth.  
 *           */  


typewriter V2 matrix (switches):
row_0 = {  1,  3,  5,  7,  8, 10, 12, 14}
row_1 = {  2,  4,  6, 20,  9, 11, 13, --}
row_2 = { 15, 17, 19, 21, 22, 24, 25, 27}
row_3 = { 16, 18, 33, 35, 23, 38, 26, 28}
row_4 = { 29, 31, 32, 34, 36, 37, 39, 41}
row_5 = { 30, 43, 45, 47, 49, 51, 40, 54}
row_6 = { 42, 44, 46, 48, 50, 52, 53, --}
row_7 = { 55, 56, 57, 58, 59, 60, 61, 62}

/* US Layout in the 60% keyboard... replacing the ~ for ESC */
/* Need to include the keys with modifiers, and the extra layouts, etc
 * AND: I could use a different encoding rspect to the linux keycodes, 
 * for example, a redundant 8th bit that means same key, but adds the deadkey flag */
 1  KEY_ESC, 
 2  KEY_1, 
 3  KEY_2, 
 4  KEY_3, 
 5  KEY_4, 
 6  KEY_5, 
 7  KEY_6, 
 8  KEY_7, 
 9  KEY_8, 
10  KEY_9, 
11  KEY_0, 
12  KEY_MINUS, 
13  KEY_EQUAL, 
14  KEY_BACKSPACE, 

15  KEY_TAB, 
16  KEY_Q, 
17  KEY_W, 
18  KEY_E, 
19  KEY_R, 
20  KEY_T, 
21  KEY_Y, 
22  KEY_U, 
23  KEY_I, 
24  KEY_O, 
25  KEY_P, 
26  KEY_LEFTBRACE, 
27  KEY_RIGHTBRACE, 
28  KEY_ENTER, 

29  KEY_CAPSLOCK, 
30  KEY_A, 
31  KEY_S, 
32  KEY_D, 
33  KEY_F, 
34  KEY_G, 
35  KEY_H, 
36  KEY_J, 
37  KEY_K, 
38  KEY_L, 
39  KEY_SEMICOLON, 
40  KEY_APOSTROPHE, 
41  KEY_BACKSLASH, 

42  KEY_LEFTSHIFT, 
43  KEY_102ND, 
44  KEY_Z, 
45  KEY_X, 
46  KEY_C, 
47  KEY_V, 
48  KEY_B, 
49  KEY_N, 
50  KEY_M, 
51  KEY_COMMA, 
52  KEY_DOT, 
53  KEY_SLASH, 
54  KEY_RIGHTSHIFT, 

55  KEY_TYPE, 
56  KEY_LEFTCTRL, 
57  KEY_LEFTALT, 
58  KEY_TYPE, 
59  KEY_SPACE, 
60  KEY_TYPE, 
61  KEY_RIGHTALT, 
62  KEY_RIGHTCTRL, 


static uint8_t keycode[256] = { \
     KEY_ESC, KEY_TAB, KEY_CAPSLOCK, KEY_LEFTSHIFT, KEY_TYPE, KEY_102ND, KEY_A, KEY_Q, KEY_1, \
       KEY_2, KEY_W, KEY_S, KEY_Z, KEY_LEFTCTRL, KEY_X, KEY_D, KEY_E, KEY_3, \
       KEY_4, KEY_R, KEY_F, KEY_C, KEY_LEFTALT, KEY_V, KEY_G, KEY_T, KEY_5, \
       KEY_6, KEY_Y, KEY_H, KEY_B, KEY_SPACE, KEY_N, KEY_J, KEY_U, KEY_7, \
       KEY_8, KEY_I, KEY_K, KEY_M, KEY_RESERVED, KEY_COMMA, KEY_L, KEY_O, KEY_9, \
       KEY_0, KEY_P, KEY_SEMICOLON, KEY_DOT, KEY_RIGHTALT, KEY_SLASH, KEY_APOSTROPHE, KEY_LEFTBRACE, KEY_MINUS, \
       KEY_EQUAL, KEY_RIGHTBRACE, KEY_BACKSLASH, KEY_RIGHTSHIFT, KEY_RIGHTCTRL, KEY_TYPE, KEY_TYPE,KEY_ENTER, KEY_BACKSPACE, \
       KEY_RESERVED, \
     KEY_GRAVE, KEY_TAB, KEY_CAPSLOCK, KEY_LEFTSHIFT, KEY_TYPE, KEY_102ND, KEY_A, KEY_Q, KEY_F1, \
       KEY_F2, KEY_W, KEY_S, KEY_Z, KEY_LEFTCTRL, KEY_X, KEY_D, KEY_E, KEY_F3, \
       KEY_F4, KEY_R, KEY_F, KEY_C, KEY_LEFTALT, KEY_V, KEY_G, KEY_T, KEY_F5, \
       KEY_F6, KEY_Y, KEY_H, KEY_B, KEY_SPACE, KEY_N, KEY_J, KEY_U, KEY_F7, \
       KEY_F8, KEY_I, KEY_K, KEY_M, KEY_RESERVED, KEY_COMMA, KEY_LEFT, KEY_O, KEY_F9, \
       KEY_F10, KEY_UP, KEY_DOWN, KEY_DOT, KEY_RIGHTALT, KEY_SLASH, KEY_RIGHT, KEY_UP, KEY_HOME, \
       KEY_END, KEY_PAGEUP, KEY_PAGEDOWN, KEY_RIGHTSHIFT, KEY_RIGHTCTRL, KEY_TYPE, KEY_TYPE,KEY_ENTER, KEY_BACKSPACE, \
       KEY_RESERVED}

