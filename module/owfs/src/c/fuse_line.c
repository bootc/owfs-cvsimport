/*
$Id$
     OW -- One-Wire filesystem
    version 0.4 7/2/2003

    Function naming scheme:
    OW -- Generic call to interaface
    LI -- LINK commands
    L1 -- 2480B commands
    FS -- filesystem commands
    UT -- utility functions
    COM - serial port functions
    DS2480 -- DS9097 serial connector

    Written 2003 Paul H Alfille
*/

#include "owfs.h"

/* Fuse_main is called with a standard C-style argc / argv parameters */

/* These routines build the char **argv */

int Fuse_setup(struct Fuse_option *fo)
{
	fo->max_options = 0;
	fo->argc = 0;
	fo->argv = NULL;
	/* create "0th" item -- the program name */
	/* Note this is required since the final NULL item doesn't exist yet */
	return Fuse_add("OWFS", fo);
}

void Fuse_cleanup(struct Fuse_option *fo)
{
	if (fo->argv) {
		char ** option_pointer;
		for (option_pointer=fo->argv; option_pointer[0] != NULL; ++option_pointer) {
			owfree(option_pointer[0]);
		}
		owfree(fo->argv);
		fo->argv = NULL ;
	}
}

int Fuse_parse(char *opts, struct Fuse_option *fo)
{
	char *start;
	char *next = opts;
	while ((start = next)) {
		strsep(&next, " ");
		if (Fuse_add(start, fo))
			return 1;
	}
	return 0;
}

/* Add an option for the fuse library. 
 * Must be in char ** argv format like any parameters sent to main
 * use a resizeable array
 */
int Fuse_add(char *opt, struct Fuse_option *fo)
{
	//LEVEL_DEBUG("Adding option %s",opt);
	if (fo->argc >= fo->max_options-1) {	
		char ** old_argv = fo->argv ;
		// need to allocate more space
		fo->max_options += 10;
		
		fo->argv = (char **) owrealloc(old_argv, (fo->max_options + 1) * sizeof(char *));
		if (fo->argv == NULL) {
			// not enough memory. Clean up and return an error
			fo->argv = old_argv ;
			Fuse_cleanup(fo) ;
			return -ENOMEM;
		}	
	}
	// Add new element
	fo->argv[fo->argc++] = owstrdup(opt);
	// and guard element at end
	fo->argv[fo->argc] = NULL;
	LEVEL_DEBUG("Added FUSE option %d %s",fo->argc-1,fo->argv[fo->argc-1]);
	return 0;
}

char *Fuse_arg(char *opt_arg, char *entryname)
{
	char *ret = NULL;
	int len = strlen(opt_arg);
	if (len < 3 || opt_arg[0] != '"' || opt_arg[len - 1] != '"') {
		fprintf(stderr, "Put the %s value in quotes. \"%s\"", entryname, opt_arg);
		return NULL;
	}
	ret = owstrdup(&opt_arg[1]);	// start after first quote
	if (ret == NULL) {
		fprintf(stderr, "Insufficient memory to store %s options: %s", entryname, opt_arg);
		return NULL;
	}
	ret[len - 2] = '\0';		// pare off trailing quote
	return ret;
}
