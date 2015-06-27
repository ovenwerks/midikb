# midikb
keyboard to Jack MIDI program

The idea of midikb is to take a Linux /dev/input/event* device and convert the keycodes to MIDI events.

This is loosely based around the Mackie Control Protocol as used with Ardour.

Aside from single events like a note value, pitch bend value or CC value, midkb will also allow the setup
of two keys as a controller or pitch bend with memory. Incoming MIDI can set this memory and the two keys
can increment or decrement the memory as well.

There are included example configuration files to let an computer keyboard emulate a 5 channel mackie
surface or extension and to use a WiiMote for remote control.
