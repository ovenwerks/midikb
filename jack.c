/*
 * Jack output for  midikb - A keyboard shortcut daemon
 *
 * Copyright (c) 2014 Len Ovens len@ovenwerks.net
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include "midikb.h"
#include <jack/jack.h>
#include <jack/midiport.h>

jack_client_t *client = NULL;
jack_port_t *output_port = NULL;
jack_port_t *input_port = NULL;
char midiin[21];
char midiout[21];
int faders[15];
int controlers[127];
int mled[3][2];


int midihex2char(char *hex)
{

/* THIS should be renamed as it just does midi out now */
/* takes string *hex and looks for two char hex values */
/* Rules:
        - first result char shall have MSB 1 all others shall be 0
        - format is a string of two char hex values with spaces between
            (ie. d0 34 7f)
        - any other stuff will log error and return result = NULL */
        
    int i, l;
    l = (strlen(hex)+3) / 3;
    for (i = 1; i<=l; i++) {
        midiout[i] = strtol(hex, &hex, 16);

    }
    midiout[0] = (char) l;


    return OK;
}

int process(jack_nframes_t nframes, void *arg)
{
    int i, s, pblsb, pbmsb, ch;
    void* inport_buf = jack_port_get_buffer(input_port, nframes);
    jack_midi_event_t in_event;
    jack_nframes_t event_count = jack_midi_get_event_count(inport_buf);
    if(event_count > 0)
    {
        for(i=0; i<event_count; i++)
        {
            jack_midi_event_get(&in_event, inport_buf, i);
            /* The events we are interested in are pitchbend for faders
            as this may be a bank change. Same for controls. The other
            event we need is note on for our LEDs */
            if( ((*(in_event.buffer) & 0xf0)) == 0xe0 )
            {
                /* pitch bend (fader) */
                ch = *(in_event.buffer) & 0x0f;
                pblsb = *(in_event.buffer + 1);
                pbmsb = *(in_event.buffer + 2);
                faders[ch] = pblsb + (pbmsb << 7);
            }
            else if( (*(in_event.buffer)) == 0xb0 )
            {
                /* controler - we are only looking at channel 1
                because that is all we have buffering set up for */
                controlers[(*(in_event.buffer + 1))] = *(in_event.buffer +2);

            }

            else if( (*(in_event.buffer)) == 0x90 )
            {
                /* note_on, we only look at this (so far) for LED
                info so there are only three notes we look for
                As we are basing this on the mackie control protocol
                we only look at channel one */
                if ((*(in_event.buffer+1)) == mled[0][0])
                    mled[0][1] = *(in_event.buffer + 2);
                else if ((*(in_event.buffer+1)) == mled[1][0])
                    mled[1][1] = *(in_event.buffer + 2);
                else if ((*(in_event.buffer+1)) == mled[2][0])
                    mled[2][1] = *(in_event.buffer + 2);
            }

        }
    }

/* output stuff below */
    void* port_buf = jack_port_get_buffer(output_port, nframes);
    unsigned char* buffer;
    jack_midi_clear_buffer(port_buf);

    if(midiout[0] != '\0')
    {
    
        s = midiout[0];
        buffer = jack_midi_event_reserve(port_buf, 0, s);
        for(i=s; i > 0; i--) {
            buffer[i - 1] = midiout[i];
        }

        midiout[0] = '\0';

    }
    return 0;
}


int jackconnect(void)
{

    if((client = jack_client_open("midikb", JackNullOption, NULL)) == 0)
        {
                fprintf(stderr, "jack server not running.\n");
                return 1;
        }

    jack_set_process_callback (client, process, 0);

    output_port = jack_port_register (client, "out", JACK_DEFAULT_MIDI_TYPE, (JackPortIsOutput | JackPortIsTerminal), 0);
    input_port = jack_port_register (client, "in", JACK_DEFAULT_MIDI_TYPE, (JackPortIsInput | JackPortIsTerminal), 0);

    if (jack_activate(client))
        {
                fprintf (stderr, "cannot activate client");
                return 1;
        }

    return 0;
}


int jackclose(void)
{
    jack_deactivate(client);
    jack_client_close(client);
    return OK;
}

int jacktransport(char *com)
{

    jack_nframes_t pos;
    pos = jack_get_current_transport_frame(client);
    char c;
    c = com[0];
    /* commands are: roll, stop, zero, forward (1 sec), Fast (10 sec) */
    /*               back (1 sec) and Back (10 sec) */
    switch (c) {
        case 'r':
            /* roll */
/*            lprintf("roll transport \n"); */
            jack_transport_start(client);
            break;

        case 's':
            /* stop */
/*            lprintf("Stop Transport \n"); */
            jack_transport_stop(client);
            break;
        
        case 'z':
            /* Zero */
            jack_transport_locate(client, 0);
            break;

        case 'f':
            /* forward 1 sec */
/*            lprintf("Forward one sec \n"); */
            jack_transport_locate(client, pos + 48000);
            break;

        case 'F':
            /* forward 10 sec */
/*            lprintf("Forward 10 sec \n"); */
            jack_transport_locate(client, pos + 480000);
            break;
        
        case 'b':
            /* back one second */
/*            lprintf("Back one second \n"); */
            if (pos < 48001)
            {
                pos = 0;
            }
            else
            {
            pos = pos - 48000;
            }
            jack_transport_locate(client, pos);
            break;
        
        case 'B':
            /* back ten second */
/*            lprintf("Back ten second \n"); */
            if (pos < 480001)
            {
                pos = 0;
            }
            else
            {
            pos = pos - 480000;
            }
            jack_transport_locate(client, pos);
            break;

        default:
            lprintf("Got a bad command\n");
        }
    return OK;
}


int fader(char *com)
{
    /* com should have a value from 0 to 15 that is the fader number. More than
    9 would be unusual (lack of keys), but there are 16 channels */
    
    int f, a, old;
    /* get fader number */
    f = strtol(com, &com, 10);
    old = faders[f];
    a = strtol(com, &com, 10) * 0x10;
    faders[f] = faders[f] + a;
    if( faders[f] > 0x3ff0) faders[f] = 0x3ff0;
    if( faders[f] < 0) faders[f] = 0;
    /* only send message if changed */
    if(faders[f] != old) {
        midiout[3] = (faders[f] & 0x3f80) >> 7;
        midiout[2] = faders[f] & 0x7f;
        midiout[1] = 0xe0 + f;
        midiout[0] = 3;  

    }

    return OK;
}

int controler(char *com)
{

    /* com should have a char from 0x0 to 0x71 that is the controler number.
    followed by the direction and number of ticks (+/-1) */
    
    int c, a, old;
    /* get channel */
    c = strtol(com, &com, 16);
    a = strtol(com, &com, 10);
    old = controlers[c];
    controlers[c] = controlers[c] + a;
    if( controlers[c] > 0x7f) controlers[c] = 0x7f;
    if( controlers[c] < 0) controlers[c] = 0;
    /* send only if value changes */
    if( controlers[c] != old ) {
        midiout[3] = controlers[c];
        midiout[2] = c;
        midiout[1] = 0xb0;
        midiout[0] = 3;  

    } 

    return OK;
}

    
