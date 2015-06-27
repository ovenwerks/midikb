#
# midikb - A keyboard shortcut and MIDI converter
#
# Copyright (c) 2005-2006 Theodoros V. Kalamatianos <nyb@users.sourceforge.net>
# Copyright (c) 2014-2015 Len Ovens <len at ovenwerks dot net>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published by
# the Free Software Foundation.
#

# prefix := /usr/local
prefix := /usr
bindir := $(prefix)/bin
sysconfdir := /etc

# Yes, I am lazy...
VER := $(shell head -n 1 NEWS | cut -d : -f 1)



DEBUG :=
CFLAGS := -O2 -Wall $(DEBUG)
CPPFLAGS := -DVERSION=\"$(VER)\" -DCONFIG=\"$(sysconfdir)/midikb.conf\"



all: midikb

midikb: midikb.o mask.o config.o linux.o jack.o -ljack

midikb.o : midikb.h
mask.o : midikb.h

config.o : midikb.h config.c

linux.o : midikb.h

jack.o : midikb.h

install: all
	install -D -m755 midikb $(bindir)/midikb
	install -d -m755 $(sysconfdir)
	echo "# midikb configuration file" > $(sysconfdir)/midikb.conf

clean:
	rm -f midikb *.o
