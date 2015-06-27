/*
 * midikb - A keyboard shortcut daemon
 *
 * Copyright (c) 2014-2015 Len Ovens <len at ovenwerks dot net>
 *
 * Based on actkbd - much of this code is still:
 * Copyright (c) 2005-2006 Theodoros V. Kalamatianos <nyb@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include "midikb.h"

/* Stuff for LED from midi and flashing */
int flash;
int mled[3][2];
int old[3];

/* Verbosity level */
int verbose = 0;

/* Daemon mode */
int detach = 0;

/* Console message suppression */
int quiet = 0;

/* Syslog logging */
int uselog = 0;

/* Maximum number of keys */
int maxkey = 0;

/* Device grab state */
int grabbed = 0;

/* Ignore release events */
int ignrel = 0;

/* Keyboard device name */
char *device = NULL;

/* Configuration file name */
char *config = NULL;

/* PID file name */
char *pidfile = NULL;



static int usage() {
    /* Len added -g line July 13,2014 */
    lprintf(
	"midikb Version %s\n"
	"Usage: midikb [options]\n"
	"    Options are as follows:\n"
	"        -c, --config <file>     Specify the configuration file to use\n"
	"        -D, --daemon            Launch in daemon mode\n"
	"        -d, --device <device>   Specify the device to use\n"
	"        -h, --help              Show this help text\n"
	"        -g, --grab              Start with device grabbed\n"
	"        -n, --noexec            Do not execute any commands\n"
	"        -p, --pidfile <file>    Use a file to store the PID\n"
	"        -q, --quiet             Suppress all console messages\n"
	"        -v[level]\n"
	"        --verbose=[level]       Specify the verbosity level (0-9)\n"
	"        -V, --version           Show version information\n"
	"        -x, --showexec          Report executed commands\n"
	"        -s, --showkey           Report key presses\n"
	"        -l, --syslog            Use the syslog facilities for logging\n"
    , VERSION);

    return OK;
}


/* Allow SIGHUP to cause reconfiguration */
void on_hup(int signum) {
    int ret;

    if ((verbose > 0) || detach)
	lprintf("Reconfiguration requested\n");

    if (verbose > 1)
	lprintf("Discarding old configuration\n");

    close_config();
    free_key_mask();
    free_ign_mask();

    if (verbose > 1)
	lprintf("Reading new configuration\n");

    if ((ret = open_config()) != OK)
	exit(ret);
    if ((ret = init_key_mask()) != OK)
	exit(ret);
    if ((ret = init_ign_mask()) != OK)
	exit(ret);

    if (verbose > 1)
	lprintf("Reconfiguration complete\n");

    return;
}


/* Allow SIGTERM to cause graceful termination */
void on_term(int signum) {
    jackclose();
    close_config();
    close_dev();
    free_key_mask();
    free_ign_mask();

    if (detach)
	lprintf("midikb %s terminating for %s\n", VERSION, device);

    closelog();

    if (pidfile != NULL)
	unlink(pidfile);

    exit(OK);

    return;
}


/* PID file creation */
static int write_pid() {
    FILE *fp;

    fp = fopen(pidfile, "w");
    if (fp == NULL) {
	lprintf("Error: could not write the PID file %s: %s\n", pidfile, strerror(errno));
	return PIDERR;
    }

    fprintf(fp, "%i\n", getpid());

    if (verbose > 2)
	lprintf("Wrote PID file %s\n", pidfile);

    fclose(fp);

    return OK;
}


/* External command execution */
static int ext_exec(char *cmd, int noexec, int showexec) {
    if ((verbose > 0) || showexec)
	lprintf("Executing: %s\n", cmd);
    if (!noexec)
	return system(cmd);

    return 0;
}


int main(int argc, char **argv) {
    int ret, key, myon, type;
    key_cmd *cmd;
/*    char midiout[21]; */
    midiout[0] = '\0';
/*    midiout = (char *)malloc(21); */

    /* Options *//* Len added grab */
    int grab = 0, help = 0, noexec = 0, version = 0, showexec = 0, showkey = 0;

    struct option options[] = {
	{ "config", required_argument, 0, 'c' },
	{ "daemon", no_argument, 0, 'D' },
	{ "device", required_argument, 0, 'd' },
	{ "grab", no_argument, 0, 'g' },
	{ "help", no_argument, 0, 'h' },
	{ "noexec", no_argument, 0, 'n' },
	{ "pidfile", required_argument, 0, 'p' },
	{ "quiet", no_argument, 0, 'q' },
	{ "verbose", optional_argument, 0, 'v' },
	{ "version", no_argument, 0, 'V' },
	{ "showexec", no_argument, 0, 'x' },
	{ "showkey", no_argument, 0, 's' },
	{ "syslog", no_argument, 0, 'l' },
	{ 0, 0, 0, 0 }
    };


    while (1) {
	int c, option_index = 0;

	/* Len added g */
	c = getopt_long (argc, argv, "c:Dd:ghp:qnv::Vxsl", options, &option_index);
	if (c == -1)
	    break;

	switch (c) {
	    case 'c':
		if (optarg) {
		    config = strdup(optarg);
		} else {
		    usage();
		    return USAGE;
		}
		break;
	    case 'D':
		detach = 1;
		break;
	    case 'd':
		if (optarg) {
		    device = strdup(optarg);
		} else {
		    usage();
		    return USAGE;
		}
		break;
	    case 'h':
		help = 1;
		break;
	    case 'g':
		grab = 1;
		break;
	    case 'n':
		noexec = 1;
		break;
	    case 'p':
		if (optarg) {
		    pidfile = strdup(optarg);
		} else {
		    usage();
		    return USAGE;
		}
		break;
	    case 'q':
		quiet = 1;
		break;
	    case 'v':
		if (optarg) {
		    verbose = optarg[0] - '0';
		    if ((verbose < 0) || (verbose > 9)) {
			usage();
			return USAGE;
		    }
		} else {
		    verbose = 1;
		}
		break;
	    case 'V':
		version = 1;
		break;
	    case 'x':
		showexec = 1;
		break;
	    case 's':
		showkey = 1;
		break;
	    case 'l':
		uselog = 1;
		break;
	    default:
		usage();
		return USAGE;
	}
    }

    if (help)
	return usage();
    if (version) {
	lprintf(
	    "midikb Version %s\n\n"
	    "Copyright (c) 2014-2015 Len Ovens <len at ovenwerks dot net>\n\n"
	    "This program is free software; you can redistribute it and/or modify it\n"
	    "under the terms of the GNU General Public License version 2 as published by\n"
	    "the Free Software Foundation.\n\n"
	    "This program is distributed in the hope that it will be useful, but WITHOUT\n"
	    "ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS\n"
	    "FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.\n"
	, VERSION);
	return OK;
    }
    if (quiet && !detach) {
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);
    }

    /* Open the syslog facility */
    if (uselog) {
	openlog("midikb", LOG_PID, LOG_DAEMON);
	uselog = 2;
    }

    /* Initialise the keyboard */
    if ((ret = init_dev()) != OK)
	return ret;

    /* open as a jack client */
    if ((ret = jackconnect()) != OK)
	return ret;

    /* Process the configuration file */
    if ((ret = open_config()) != OK)
	return ret;

    if ((ret = init_key_mask()) != OK)
	return ret;

    if ((ret = init_ign_mask()) != OK)
	return ret;

    if ((ret = open_dev()) != OK)
	return ret;

    /* Len added this to start with device grabbed */
    if (grab)
	grab_dev();

    /* Verbosity levels over 2 make showkey redundant */
    if (verbose > 2)
	showkey = 0;

    if (detach) {
		switch (daemon(0, 0))
		{
			case 0:
				break;
			default:
				lprintf("%s: fork() error: %s\n", argv[0], strerror(errno));
				return FORKERR;
		}
		lprintf("midikb %s launched for %s\n", VERSION, device);
    }

    if (pidfile != NULL)
	if ((ret = write_pid()) != OK)
	    return ret;

    /* Setup the signal handlers */
	/* try to catch anything so jack is happy */
    signal(SIGHUP, on_hup);
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);    
    signal(SIGPIPE, on_term);    
    signal(SIGQUIT, on_term);    
    signal(SIGKILL, on_term);    
    signal(SIGABRT, on_term);    

	/* make sure all LEDs are off at start */
	set_led(0, 0);
	set_led(1, 0);
	set_led(2, 0);


    while (get_key(&key, &type) == OK) {
	int tmp, exec_ok = 0, norel = 0;

	if ((type & (KEY | REP)) != 0)
	    set_key_bit(key, 1);

	if (verbose > 2) {
	    lprintf("Event: ");
	    lprint_key_mask();
	    lprintf(":%s\n", (type == KEY)?"key":((type == REP)?"rep":"rel"));
	}
	if ((type == KEY) && showkey) {
	    lprintf("Keys: ");
	    lprint_key_mask();
	    lprintf("\n");
	}

	ret = match_key(type, &cmd);
	if (ret == OK) {
	    attr_t *attr;

	    /* Attribute implementation */
	    attr = cmd->attrs;
	    while (attr != NULL) {
		char *str, opt[32] = { '\0' };
		int out_type = INVALID;
		switch (attr->type) {
		    case ATTR_EXEC:
			str = "exec";
			ext_exec(cmd->command, noexec, showexec);
			exec_ok = 1;
			break;
		    case ATTR_JACKT:
		        /* Len adds jack transport line */
			str = "jackt";
			jacktransport(cmd->command);
			break;
		    case ATTR_JACKM:
		        /* Len adds Jack MIDI Line */
			str = "jackm";
			midihex2char(cmd->command);
			break;
            case ATTR_FADER:
                /* This is an encoder like signal ticks up or down */
                /* Command form is "f [+/-]n" 
                   f = fader 1 to 16 
                   n = number of ticks < 512 (generally a lot less) */
                str = "fader";
                fader(cmd->command);
                break;
			case ATTR_CNTRL:

				/* take encoder tick and make a numeric midi control.
                command form is "c [=/-]n" 
                c = midi controler number 00 to 79 hex 
                n is number of ticks < 127 */
                str = "control";
                controler(cmd->command);
                break;
		    case ATTR_GRAB:
			str = "grab";
			grab_dev();
			break;
		    case ATTR_UNGRAB:
			str = "ungrab";
			ungrab_dev();
			break;
		    case ATTR_IGNREL:
			str = "ignrel";
			copy_key_to_ign_mask();
			ignrel = 1;
			break;
		    case ATTR_RCVREL:
			str = "rcvrel";
			ignrel = 0;
			break;
		    case ATTR_ALLREL:
			str = "allrel";
			clear_key_mask();
			break;
		    case ATTR_KEY:
			str = "key";
			out_type = KEY;
			break;
		    case ATTR_REL:
			str = "rel";
			out_type = REL;
			break;
		    case ATTR_REP:
			str = "rep";
			out_type = REP;
			break;
		    case ATTR_SET:
			str = "set";
			tmp = (int)(long)(attr->opt);
			if (tmp < 0)
			    tmp = key;
			if (tmp == key)
			    norel = 1;
			snprintf(opt, 32, "%i", tmp);
			set_key_bit(tmp, 1);
			break;
		    case ATTR_UNSET:
			str = "unset";
			tmp = (((int)(long)(attr->opt)) >= 0)?(int)(long)(attr->opt):key;
			snprintf(opt, 32, "%i", tmp);
			set_key_bit(tmp, 0);
			break;
		    case ATTR_LEDON:
			str = "ledon";
			snprintf(opt, 32, "%i", (int)(long)(attr->opt));
			set_led((int)(long)(attr->opt), 1);
			break;
		    case ATTR_LEDOFF:
			str = "ledoff";
			snprintf(opt, 32, "%i", (int)(long)(attr->opt));
			set_led((int)(long)(attr->opt), 0);
			break;
		    default:
			str = "unknown";
			break;
		}

		if (out_type != INVALID) {
		    tmp = (((int)(long)(attr->opt)) >= 0)?(int)(long)(attr->opt):key;
		    snprintf(opt, 32, "%i", tmp);
		    snd_key(tmp, out_type);
		}

		if ((verbose > 0) || showexec)
		    lprintf("Attribute: %s(%s)\n", str, opt);

		attr = attr->next;
	    }

	    /* Fall back on command execution */
	    if ((!exec_ok) && ((cmd->attr_bits & BIT_ATTR_NOEXEC) == 0)) {
		ext_exec(cmd->command, noexec, showexec);
		exec_ok = 1;
	    }
	}

	if ((type == REL) && (!norel) && ((!ignrel) || (get_ign_bit(key) == 0)))
	    set_key_bit(key, 0);

/*	We want to change this for sure, no timer, non-blocking loop */
/*	this last section seems to be the only reliable way to turn on LEDs
        because of MIDI commands received in response to a key press. However,
        we only come here at an input event. If the DAW sends a LED on
        event on it's own, we don't see it till the next key press */ 
/*	We add a delay to the release events so that a midi signal has time
        to get to the controled program and back to turn on a LED */
        if (type == REL)
            usleep(70000);
        /* 100000 works */
        if (old[0] != mled[0][1])
        {
            myon = 1;
            if (mled[0][1] == 0)
                myon = 0;
            set_led(0, myon);
            old[0] = mled[0][1];
        }
        if (old[1] != mled[1][1])
        {
            myon = 1;
            if (mled[1][1] == 0)
                myon = 0;
            set_led(1, myon);
            old[1] = mled[1][1];
        }
        if (old[2] != mled[2][1])
        {
            myon = 1;
            if (mled[2][1] == 0)
                myon = 0;
            set_led(2, myon);
            old[2] = mled[2][1];
        }

    }

    return OK;
}


/* Logging function */
int lprintf(const char *fmt, ...) {
    va_list args;
    int ret = 0;

    va_start(args, fmt);
    if (!quiet)
	ret = vfprintf(stderr, fmt, args);
    if (uselog == 2)
	vsyslog(LOG_NOTICE, fmt, args);
    va_end(args);

    return ret;
}
