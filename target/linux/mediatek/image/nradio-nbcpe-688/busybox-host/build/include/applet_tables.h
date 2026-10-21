/* This is a generated file, don't edit */

#define NUM_APPLETS 2
#define KNOWN_APPNAME_OFFSETS 0

const char applet_names[] ALIGN1 = ""
"awk" "\0"
"sh" "\0"
;

#define APPLET_NO_awk 0
#define APPLET_NO_sh 1

#ifndef SKIP_applet_main
int (*const applet_main[])(int argc, char **argv) = {
awk_main,
ash_main,
};
#endif

