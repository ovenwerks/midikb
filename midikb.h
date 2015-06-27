/*
 * midikb - A keyboard shortcut daemon
 *
 * Copyright (c) 2014-2015 Len Ovens <len at ovenwerks dot net>
 * Copyright (c) 2005-2006 Theodoros V. Kalamatianos <nyb@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _MIDIKB_H_
#define _MIDIKB_H_

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <getopt.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>

/* Len's additions */
#include <jack/jack.h>
#include <jack/midiport.h>
/* #include <jack/transport.h> */


#define UNUSED 0


/* Event types */
#define INVALID		0
#define KEY		(1<<0)
#define REP		(1<<1)
#define REL		(1<<2)
#define LED		(1<<3)

/* Return values */
enum { OK, USAGE, MEMERR, HOSTFAIL, DEVFAIL, READERR, WRITEERR, EVERR, CONFERR,
	FORKERR, INTERR, PIDERR, NOMATCH };


/* Jack client handle */
extern jack_client_t *client;
extern jack_port_t *output_port;
extern jack_port_t *input_port;

/* strings to hold incoming midi events to turn LEDs on/off */
/* mled[0-2][] is LEDs 1 to 3
    mled[][0] is controller number (0 to 121)
    mled[][1] is LED state - 127 = on 0 = off and 1 = flash */
extern int mled[3][2];
/* we support midi commands up to 20 char
    [0] holds the number of char because midi uses 00 as a valid char
    really we only look at 3 char on MIDI in right now */
extern char midiin[21];
extern char midiout[21];

/* arrays to hold faders and controllers */
extern int faders[15];
extern int controlers[127];

/* Verbosity level */
extern int verbose;

/* Maximum number of keys */
extern int maxkey;

/* Device grab state */
extern int grabbed;

/* The device name */
extern char *device;

/* The configuration file name */
extern char *config;


/* Logging function */
int lprintf(const char *fmt, ...);

/* Device initialisation */
int init_dev();

/* Device open function */
int open_dev();

/* Device close function */
int close_dev();

/* Device grab function */
int grab_dev();

/* Device un-grab function */
int ungrab_dev();

/* add some jack/midi stuff */

/* config midi input catch events */
/*int midiledcfg(char *line); */

/* Jackd client open */
int jackconnect();

/* Jackd Transport command send */
int jacktransport(char *com);

/* convert text string to midi string */
/* hm we have mde this midiout... */
int midihex2char(char *hex);

/* Take encoder ticks and make a Pitchbend value */
int fader(char *com);

/* Take encoder ticks and make a controler value */
int controler(char *com);

/* Jackd client close */
int jackclose();

/* end of added jack/midi stuff */

/* Keyboard event receiver function */
int get_key(int *key, int *type);

/* Send an event to the input layer */
int snd_key(int key, int type);

/* Set a keyboard LED */
int set_led(int led, int on);


/* Key mask handling */
int get_masksize();
int init_mask(unsigned char **mask);
void free_mask(unsigned char **mask);
int lprint_mask(unsigned char *mask);
int strmask(unsigned char **mask, char *keys);

/* The active key mask */
int init_key_mask();
void free_key_mask();
void clear_key_mask();
int set_key_bit(int bit, int val);
int get_key_bit(int bit);
int cmp_key_mask(unsigned char *mask0, unsigned int attr);
int lprint_key_mask_delim(char c);
int lprint_key_mask();
unsigned char *get_key_mask();

/* The ignored key mask */
int init_ign_mask();
void free_ign_mask();
void clear_ign_mask();
int set_ign_bit(int bit, int val);
int get_ign_bit(int bit);
int cmp_ign_mask(unsigned char *mask0, unsigned int attr);
int lprint_ign_mask_delim(char c);
int lprint_ign_mask();
unsigned char *get_ign_mask();
void copy_key_to_ign_mask();


/* The attribute node struct */
typedef struct _attr_t attr_t;
struct _attr_t {
    int type;			/* Attribute type */
/*    void *opt;	*/		/* Options for this attribute */
	int opt;
    attr_t *next;		/* The next node */
};

/* Supported attributes */
/* Len adds some more */
#define ATTR_EXEC		0
#define ATTR_GRAB		1
#define ATTR_UNGRAB		2
#define ATTR_IGNREL		3
#define ATTR_RCVREL		4
#define ATTR_ALLREL		5
#define ATTR_KEY		6
#define ATTR_REL		7
#define ATTR_REP		8
#define ATTR_LEDON		9
#define ATTR_LEDOFF		10
#define ATTR_SET		11
#define ATTR_UNSET		12
#define ATTR_JACKT		13
#define ATTR_JACKM		14
#define ATTR_FADER		15
#define ATTR_CNTRL		16


/* The key_cmd struct */
typedef struct {
    unsigned char *keys;	/* The key mask */
    int type;			/* The event type */
    char *command;		/* The command to execute */

    unsigned int attr_bits;	/* Bitwise attributes */

    attr_t *attrs;		/* The attribute list */
} key_cmd;

/* The bitwise attribute values */
#define BIT_ATTR_NOEXEC		(1<<0)	/* Do not call system() */
#define BIT_ATTR_GRABBED	(1<<1)	/* Match only when the device is grabbed */
#define BIT_ATTR_UNGRABBED	(1<<2)	/* Match only when the device is not grabbed */
#define BIT_ATTR_NOT		(1<<3)	/* Match any key except for the specified ones */
#define BIT_ATTR_ALL		(1<<4)	/* Match if all of the specified keys is pressed */
#define BIT_ATTR_ANY		(1<<5)	/* Match if any of the specified keys is pressed */


/* Configuration file processing */
int open_config();
int close_config();
int match_key(int type, key_cmd **command);


#endif /* _MIDIKB_H_ */
