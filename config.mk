VERSION = 0.0

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

CC = cc
LD = $(CC)
AR = ar
RANLIB = ranlib

# For NetBSD add -D_NETBSD_SOURCE
CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_GNU_SOURCE -D_XOPEN_SOURCE=700
CFLAGS   = -std=gnu11 -Wall -pedantic -I./libysp -O0 -g
#CFLAGS   = -std=gnu11 -Wall -pedantic -I./libysp -O3 -g
LDFLAGS  = -g -lrt -lm

PARETO ?= 1
CFLAGS += -DPARETO=$(PARETO)

EXPANDALLTRACE ?= 0
ifeq "$(EXPANDALLTRACE)" "1"
	CFLAGS += -DEXPANDALLTRACE
endif
