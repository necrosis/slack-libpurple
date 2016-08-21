LIBNAME = slack_plugin.so

.PHONY: all
all: $(LIBNAME)

C_SRCS = cstring.c json.c slack_messages.c slack_chat.c slack_plugin.c

# Object file names using 'Substitution Reference'
C_OBJS = $(C_SRCS:.c=.o)

CC = gcc
LD = $(CC)
CFLAGS_PURPLE = $(shell pkg-config --cflags purple)
CFLAGS = \
    -g \
    -O2 \
    -Wall \
    -Wextra \
    -fPIC \
    -DPURPLE_PLUGINS \
    -DPIC -DENABLE_NLS \
    -std=c99 \
    $(CFLAGS_PURPLE)

INCLUDES_CURL = -I/home/necrosis/development/usr/include

LIBS_PURPLE = $(shell pkg-config --libs purple)
LIBS_CURL = -L/home/necrosis/development/usr/lib -lcurl -lm #$(shell pkg-config --libs curl)
LDFLAGS = -shared

%.o: %.c
	$(V_CC)$(CC) -c $(CFLAGS) -o $@ $< $(INCLUDES_CURL)

$(LIBNAME): $(C_OBJS)
	$(V_LINK)$(LD) $(LDFLAGS) -o $@ $^ $(LIBS_PURPLE) $(LIBS_CURL)

PLUGIN_DIR_PURPLE:=$(shell pkg-config --variable=plugindir purple)
DATA_ROOT_DIR_PURPLE:=$(shell pkg-config --variable=datarootdir purple)

.PHONY: install
install: $(LIBNAME)
	install -D $(LIBNAME) $(PLUGIN_DIR_PURPLE)/$(LIBNAME)
	install --mode=0644 img/slack16.png $(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/16/slack.png
	install --mode=0644 img/slack22.png $(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/22/slack.png
	install --mode=0644 img/slack48.png $(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/48/slack.png

.PHONY: uninstall
uninstall: $(LIBNAME)
	rm $(PLUGIN_DIR_PURPLE)/$(LIBNAME)
	rm $(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/16/slack.png
	rm $(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/22/slack.png
	rm $(DATA_ROOT_DIR_PURPLE)/pixmaps/pidgin/protocols/48/slack.png

.PHONY: clean
clean:
	-rm *.o
	-rm $(LIBNAME)


# Quiet by default
VERBOSE ?= 0

# Define printf macro
v_printf = @printf "  %-8s%s\n"

# Define C verbose macro
V_CC = $(v_CC_$(V))
v_CC_ = $(v_CC_$(VERBOSE))
v_CC_0 = $(v_printf) CC $(@F);

# Define LINK verbose macro
V_LINK = $(v_LINK_$(V))
v_LINK_ = $(v_LINK_$(VERBOSE))
v_LINK_0 = $(v_printf) LINK $(@F);
