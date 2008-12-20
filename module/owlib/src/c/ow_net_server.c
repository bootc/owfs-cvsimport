/*
$Id$
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: palfille@earthlink.net
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

/* ow_net holds the network utility routines. Many stolen unashamedly from Steven's Book */
/* Much modification by Christian Magnusson especially for Valgrind and embedded */
/* non-threaded fixes by Jerry Scharf */

#include <config.h>
#include "owfs_config.h"
#include "ow.h"
#include "ow_counters.h"
#include "ow_connection.h"

/* Prototypes */
static int ServerAddr(const char * default_port, struct connection_out *out);
static int ServerListen(struct connection_out *out);


static int ServerAddr(const char * default_port, struct connection_out *out)
{
	struct addrinfo hint;
	int ret;
	char *p;

	if (out->name == NULL) {	// use defaults
		out->host = strdup("0.0.0.0");
		out->service = strdup(default_port);
	} else if ((p = strrchr(out->name, ':')) == NULL) {
		if (strchr(out->name, '.')) {	//probably an address
			out->host = strdup(out->name);
			out->service = strdup(default_port);
		} else {				// assume a port
			out->host = strdup("0.0.0.0");
			out->service = strdup(out->name);
		}
	} else {
		p[0] = '\0';
		out->host = strdup(out->name);
		p[0] = ':';
		out->service = strdup(&p[1]);
	}
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_flags = AI_PASSIVE;
	hint.ai_socktype = SOCK_STREAM;
#if OW_CYGWIN || defined(__FreeBSD__)
	hint.ai_family = AF_INET;	// FreeBSD will bind IP6 preferentially
#else							/* __FreeBSD__ */
	hint.ai_family = AF_UNSPEC;
#endif							/* __FreeBSD__ */

	//printf("ServerAddr: [%s] [%s]\n", out->host, out->service);

	if ((ret = getaddrinfo(out->host, out->service, &hint, &out->ai))) {
		ERROR_CONNECT("GetAddrInfo error [%s]=%s:%s\n", SAFESTRING(out->name), SAFESTRING(out->host), SAFESTRING(out->service));
		return -1;
	}
	return 0;
}

static int ServerListen(struct connection_out *out)
{
	if (out->ai == NULL) {
		LEVEL_CONNECT("Server address not yet parsed [%s]\n", SAFESTRING(out->name));
		return -EIO;
	}

	if (out->ai_ok == NULL) {
		out->ai_ok = out->ai;
	}

	do {
		int on = 1;
		int file_descriptor = socket(out->ai_ok->ai_family, out->ai_ok->ai_socktype, out->ai_ok->ai_protocol);

		//printf("ServerListen file_descriptor=%d\n",file_descriptor);
		if (file_descriptor < 0) {
			ERROR_CONNECT("ServerListen: Socket problem [%s]\n", SAFESTRING(out->name));
		} else if (setsockopt(file_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) != 0) {
			ERROR_CONNECT("ServerListen: SetSockOpt problem [%s]\n", SAFESTRING(out->name));
		} else if (bind(file_descriptor, out->ai_ok->ai_addr, out->ai_ok->ai_addrlen) != 0) {
			// this is where the default linking to a busy port shows up
			ERROR_CONNECT("ServerListen: Bind problem [%s]\n", SAFESTRING(out->name));
		} else if (listen(file_descriptor, 10) != 0) {
			ERROR_CONNECT("ServerListen: Listen problem [%s]\n", SAFESTRING(out->name));
		} else {
			out->file_descriptor = file_descriptor;
			return file_descriptor;
		}
		if ( file_descriptor >= 0 ) {
			close( file_descriptor );
		}
	} while ((out->ai_ok = out->ai_ok->ai_next));
	LEVEL_CONNECT("ServerListen: No good listen network sockets [%s]\n", SAFESTRING(out->name));
	return -1;
}

int ServerOutSetup(struct connection_out *out)
{
	if ( out->name == NULL ) { // NULL name means default attempt
		char * default_port ;
		// First time through, try default port
		switch (Globals.opt) {
			case opt_server:
				default_port = "4304" ;
				break ;
			case opt_ftpd:
				default_port = "21" ;
				break ;
			default:
				default_port = NULL ;
				break ;
		}
		if ( default_port != NULL ) { // one of the 2 cases above 
			if ( ServerAddr( default_port, out ) < 0 ) {
				return -1 ;
			}
			if ( ServerListen(out)  >= 0 ) {
				return 0 ;
			}
			ERROR_CONNECT("Default port not successful. Try an ephemeral port\n");
		}
	}

	// second time through, use ephemeral port
	if ( ServerAddr( "0", out ) < 0 ) {
		return -1 ;
	}
	return (ServerListen(out)<0) ? -1 : 0 ;
}

/*
 Loops through count_outbound_connections, starting a detached thread for each 
 Each loop spawn threads for accepting connections
 Uses my non-patented "pre-threaded technique"
 */

#if OW_MT

/* structures */
struct HandlerThread_data {
	int acceptfd;
	void (*HandlerRoutine) (int file_descriptor);
};

void *ServerProcessHandler(void *arg)
{
	struct HandlerThread_data *hp = (struct HandlerThread_data *) arg;
	pthread_detach(pthread_self());
	if (hp) {
		hp->HandlerRoutine(hp->acceptfd);
		/* This will never be reached right now.
		   The thread is killed when application is stopped.
		   Should perhaps fix a signal handler for this. */
		Test_and_Close( &(hp->acceptfd) );
		free(hp);
	}
	pthread_exit(NULL);
	return NULL;
}

static void ServerProcessAccept(void *vp)
{
	struct connection_out *out = (struct connection_out *) vp;
	pthread_t tid;
	int acceptfd;
	int ret;

	LEVEL_DEBUG("ServerProcessAccept %s[%lu] try lock %d\n", SAFESTRING(out->name), (unsigned long int) pthread_self(), out->index);

	ACCEPTLOCK(out);

	LEVEL_DEBUG("ServerProcessAccept %s[%lu] locked %d\n", SAFESTRING(out->name), (unsigned long int) pthread_self(), out->index);

	do {
		acceptfd = accept(out->file_descriptor, NULL, NULL);
		if (StateInfo.shutdown_in_progress) {
			LEVEL_DEBUG("ServerProcessAccept shutdown_in_progress %s[%lu] accept %d\n",SAFESTRING(out->name),(unsigned long int)pthread_self(),out->index) ;
			break;
		}
		LEVEL_DEBUG("ServerProcessAccept %s[%lu] accept %d fd=%d\n",SAFESTRING(out->name),(unsigned long int)pthread_self(),out->index, acceptfd) ;
		if (acceptfd < 0) {
			if (errno == EINTR) {
				LEVEL_DEBUG("ow_net_server.c: accept interrupted\n");
				continue;
			}
			LEVEL_DEBUG("ow_net_server.c: accept error %d [%s]\n", errno, strerror(errno));
		}
		break;
	} while (1);
	ACCEPTUNLOCK(out);
	
	LEVEL_DEBUG("ServerProcessAccept %s[%lu] unlock %d\n",SAFESTRING(out->name),(unsigned long int)pthread_self(),out->index) ;

	if (StateInfo.shutdown_in_progress) {
		LEVEL_DEBUG
			("ServerProcessAccept %s[%lu] shutdown_in_progress %d return\n", SAFESTRING(out->name), (unsigned long int) pthread_self(), out->index);
		if (acceptfd >= 0) {
			close(acceptfd);
		}
		return;
	}

	if (acceptfd < 0) {
		ERROR_CONNECT("ServerProcessAccept: accept() problem %d (%d)\n", out->file_descriptor, out->index);
	} else {
		struct HandlerThread_data *hp;
		hp = malloc(sizeof(struct HandlerThread_data));
		if (hp) {
			hp->HandlerRoutine = out->HandlerRoutine;
			hp->acceptfd = acceptfd;
			ret = pthread_create(&tid, NULL, ServerProcessHandler, hp);
			if (ret) {
				LEVEL_DEBUG("ServerProcessAccept %s[%lu] create failed ret=%d\n", SAFESTRING(out->name), (unsigned long int) pthread_self(), ret);
				close(acceptfd);
				free(hp);
			}
		}
	}
	LEVEL_DEBUG("ServerProcessAccept = %lu CLOSING\n", (unsigned long int) pthread_self());
	return;
}

void ServerProcessCleanup(void *param)
{
  struct connection_out *out = (struct connection_out *)param;
  LEVEL_DEBUG("ServerProcessCleanup: cleaning up blocked thread %d\n", out->tid);
  ACCEPTUNLOCK(out);
  LEVEL_DEBUG("ServerProcessCleanup: unlocked accept mutex\n");
}

/* For a given port (connecion_out) set up listening */
static void *ServerProcessOut(void *v)
{
	struct connection_out *out = (struct connection_out *) v;

	LEVEL_DEBUG("ServerProcessOut = %lu\n", (unsigned long int) pthread_self());

	if (ServerOutSetup(out)) {
		LEVEL_CONNECT("Cannot set up server socket on %s, index=%d -- will exit\n", SAFESTRING(out->name), out->index);
		(out->Exit) (1);
	}

	OW_Announce(out);

	pthread_cleanup_push(ServerProcessCleanup, v);
	
	while (!StateInfo.shutdown_in_progress) {
		ServerProcessAccept(v);
	}

	LEVEL_DEBUG("ServerProcessOut = %lu CLOSING (%s)\n", (unsigned long int) pthread_self(), SAFESTRING(out->name));

	pthread_cleanup_pop(0);

	OUTLOCK(out);
	out->tid = 0;
	OUTUNLOCK(out);
	pthread_exit(NULL);
	return NULL;
}

/* Setup Servers -- a thread for each port */
void ServerProcess(void (*HandlerRoutine) (int file_descriptor), void (*Exit) (int errcode))
{
	struct connection_out *out ;
	int err, signo;
	sigset_t myset;

	if (Outbound_Control.active == 0) {
		LEVEL_CALL("No output devices defined\n");
		Exit(1);
	}

	/* Start the head of a thread chain for each head_outbound_list */
	for (out = Outbound_Control.head; out; out = out->next) {
		OUTLOCK(out);
		out->HandlerRoutine = HandlerRoutine;
		out->Exit = Exit;
		if (pthread_create(&(out->tid), NULL, ServerProcessOut, (void *) (out))) {
			OUTUNLOCK(out);
			ERROR_CONNECT("Could not create a thread for %s\n", SAFESTRING(out->name));
			Exit(1);
		}
		OUTUNLOCK(out);
	}

	(void) sigemptyset(&myset);
	(void) sigaddset(&myset, SIGHUP);
	(void) sigaddset(&myset, SIGINT);
	(void) sigaddset(&myset, SIGTERM);
	(void) pthread_sigmask(SIG_BLOCK, &myset, NULL);

	while (!StateInfo.shutdown_in_progress) {
		if ((err = sigwait(&myset, &signo)) == 0) {
			if (signo == SIGHUP) {
				LEVEL_DEBUG("ServerProcess: ignore signo=%d\n", signo);
				continue;
			}
			LEVEL_DEBUG("ServerProcess: break signo=%d\n", signo);
			break;
		} else {
			LEVEL_DEBUG("ServerProcess: sigwait error %n\n", err);
		}
	}
	LEVEL_DEBUG("ow_net_server.c:ServerProcess() shutdown initiated\n");

	for (out = Outbound_Control.head; out; out = out->next) {
		OUTLOCK(out);
		if (out->tid) {
			LEVEL_DEBUG("Shutting down %d of %d thread %ld\n", out->index, Outbound_Control.active, out->tid);
			if (pthread_cancel(out->tid)) {
				LEVEL_DEBUG("Can't kill %d of %d\n", out->index, Outbound_Control.active);
			}
		}
		OUTUNLOCK(out);
	}

	LEVEL_DEBUG("ow_net_server.c:ServerProcess() all threads cancelled\n");

	for (out = Outbound_Control.head; out; out = out->next) {
		pthread_join(out->tid, NULL);
		out->tid = 0;
	}

	LEVEL_DEBUG("ow_net_server.c:ServerProcess() shutdown done\n");

	/* Cleanup that may never be reached */
	Exit(0);
}

#else							/* OW_MT */

// Non multithreaded
void ServerProcess(void (*HandlerRoutine) (int file_descriptor), void (*Exit) (int errcode))
{
	if (Outbound_Control.active == 0) {
		LEVEL_CONNECT("No output device (port) specified. Exiting.\n");
		Exit(1);
	} else if (Outbound_Control.active > 1) {
		LEVEL_CONNECT("More than one output device specified (%d). Library compiled non-threaded. Exiting.\n", Outbound_Control.active);
		Exit(1);
	} else if (ServerOutSetup(Outbound_Control.head)) {
		LEVEL_CONNECT("Cannot set up head_outbound_list [%s] -- will exit\n", SAFESTRING(head_outbound_list->name));
		Exit(1);
	} else {
		OW_Announce(Outbound_Control.head);
		while (1) {
			int acceptfd = accept(Outbound_Control.head->file_descriptor, NULL, NULL);
			if (StateInfo.shutdown_in_progress) {
				break;
			}
			if (acceptfd < 0) {
				ERROR_CONNECT("Trouble with accept, will reloop\n");
			} else {
				HandlerRoutine(acceptfd);
				close(acceptfd);
			}
		}
		Exit(0);
	}
}
#endif							/* OW_MT */
