/*
$Id$
 * http.c for owhttpd (1-wire web server)
 * By Paul Alfille 2003, using libow
 * offshoot of the owfs ( 1wire file system )
 *
 * GPL license ( Gnu Public Lincense )
 *
 * Based on chttpd. copyright(c) 0x7d0 greg olszewski <noop@nwonknu.org>
 *
 */

#include "owhttpd.h"

// #include <libgen.h>  /* for dirname() */

/* --------------- Functions ---------------- */
static void ShowDirText(FILE * out, struct parsedname * pn);
static void ShowDirJson(FILE * out, struct parsedname * pn);
/* Find he next higher level by search for last slash that doesn't end the string */
/* return length of "higher level" */
/* Assumes good path:
    null delimitted
    starts with /
    non-null
*/
int Backup(const char *path)
{
	int i = strlen(path) - 1;
	if (i == 0)
		return 1;
	if (path[i] == '/')
		--i;
	while (path[i] != '/')
		--i;
	return i + 1;
}

#define name_directory    "directory"
#define name_onewire_chip "1-wire chip"

	/* Callback function to FS_dir */
static void ShowDirCallback(void *v, const struct parsedname *pn_entry)
{
	/* uncached tag */
	/* device name */
	/* Have to allocate all buffers to make it work for Coldfire */
	FILE *out = v;
	const char *nam;
	const char *typ;
	char * escaped_path = httpescape( pn_entry->path ) ;

	if (IsDir(pn_entry)) {
		nam = FS_DirName(pn_entry);
		typ = name_directory;
	} else {
		nam = pn_entry->selected_device->readable_name;
		typ = name_onewire_chip;
	}

	fprintf(out,
			"<TR><TD><A HREF='%s'><CODE><B><BIG>%s</BIG></B></CODE></A></TD><TD>%s</TD><TD>%s</TD></TR>", escaped_path==NULL ? pn_entry->path : escaped_path, FS_DirName(pn_entry), nam,
			typ);

	if ( escaped_path ) {
		owfree( escaped_path ) ;
	}
}

	/* Misnamed. Actually all directory */
void ShowDir(FILE * out, struct parsedname * pn)
{
	int b = Backup(pn->path); // length of string to get to higher level
	if (pn->state & ePS_text) {
		ShowDirText(out, pn);
		return;
	} else if (pn->state & ePS_json) {
		ShowDirJson(out, pn);
		return;
	}

	HTTPstart(out, "200 OK", ct_html);
	HTTPtitle(out, "Directory");

	if (NotRealDir(pn)) {
		/* return whole path since tree structure could be much deeper now */
		/* first / is stripped off */
		HTTPheader(out, &pn->path[1]);
	} else if (pn->state) {
		/* return whole path since tree structure could be much deeper now */
		/* first / is stripped off */
		HTTPheader(out, &pn->path[1]);
	} else {
		HTTPheader(out, "directory");
	}


	fprintf(out, "<TABLE BGCOLOR=\"#DDDDDD\" BORDER=1>");

	if (b != 1) {
		char * escaped_path = httpescape( pn->path ) ;
		fprintf(out, "<TR><TD><A HREF='%.*s'><CODE><B><BIG>up</BIG></B></CODE></A></TD><TD>higher level</TD><TD>directory</TD></TR>", b, escaped_path==NULL ? pn->path : escaped_path );
		if ( escaped_path ) {
			owfree( escaped_path ) ;
		}
	} else {
		fprintf(out, "<TR><TD><A HREF='/'><CODE><B><BIG>top</BIG></B></CODE></A></TD><TD>highest level</TD><TD>directory</TD></TR>");
	}

	FS_dir(ShowDirCallback, out, pn);
	fprintf(out, "</TABLE>");
	HTTPfoot(out);
}

static void ShowDirTextCallback(void *v, const struct parsedname *const pn_entry)
{
	/* uncached tag */
	/* device name */
	/* Have to allocate all buffers to make it work for Coldfire */
	FILE *out = v;
	const char *nam;
	const char *typ;
	if (IsDir(pn_entry)) {
		nam = FS_DirName(pn_entry);
		typ = name_directory;
	} else {
		nam = pn_entry->selected_device->readable_name;
		typ = name_onewire_chip;
	}
	fprintf(out, "%s %s \"%s\"\r\n", FS_DirName(pn_entry), nam, typ);
}

static void ShowDirText(FILE * out, struct parsedname * pn)
{
	HTTPstart(out, "200 OK", ct_text);

	FS_dir(ShowDirTextCallback, out, pn);
	return;
}

static void ShowDirJsonCallback(void *v, const struct parsedname *const pn_entry)
{
	/* uncached tag */
	/* device name */
	/* Have to allocate all buffers to make it work for Coldfire */
	FILE *out = v;
	const char *nam;

	if (IsDir(pn_entry)) {
		nam = FS_DirName(pn_entry);
	} else {
		nam = pn_entry->selected_device->readable_name;
	}
	fprintf(out, "\"%s\":[],\n", nam ) ;
}

static void ShowDirJson(FILE * out, struct parsedname * pn)
{
	HTTPstart(out, "200 OK", ct_text);

	fprintf(out, "{" );
	FS_dir(ShowDirJsonCallback, out, pn);
	fprintf(out, "}" );
	return;
}
