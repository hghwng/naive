/*
 * login(1)
 *
 * This program is derived from 4.3 BSD software and is subject to the
 * copyright notice below.
 *
 * Michael Glad (glad@daimi.dk)
 * Computer Science Department, Aarhus University, Denmark
 * 1990-07-04
 *
 * Copyright (c) 1980, 1987, 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <memory.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <utmp.h>
#include <setjmp.h>
#include <stdlib.h>
#include <locale.h>
#include <libintl.h>
#include <sys/syslog.h>
#include <sys/sysmacros.h>
#include <linux/major.h>
#include <netdb.h>
#include <stdarg.h>
#ifdef HAVE_LIBAUDIT
# include <libaudit.h>
#endif

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#ifdef HAVE_SECURITY_PAM_MISC_H
#  include <security/pam_appl.h>
#  include <security/pam_misc.h>
#  define PAM_MAX_LOGIN_TRIES	3
#  define PAM_FAIL_CHECK if (retcode != PAM_SUCCESS) { \
       fprintf(stderr,"\n%s\n",pam_strerror(pamh, retcode)); \
       syslog(LOG_ERR,"%s",pam_strerror(pamh, retcode)); \
       pam_end(pamh, retcode); exit(EXIT_FAILURE); \
   }
#  define PAM_END { \
	pam_setcred(pamh, PAM_DELETE_CRED); \
	retcode = pam_close_session(pamh,0); \
	pam_end(pamh,retcode); \
}
#endif

#include <lastlog.h>

#define SLEEP_EXIT_TIMEOUT 5

#ifndef HAVE_SECURITY_PAM_MISC_H
static void getloginname (void);
static void checknologin (void);
static int rootterm (char *ttyn);
#endif
static void timedout (int);
static void sigint (int);
static void motd (void);
static void dolastlog (int quiet);

#ifdef USE_TTY_GROUP
#  define TTY_MODE 0620
#else
#  define TTY_MODE 0600
#endif

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */

#ifndef MAXPATHLEN
#  define MAXPATHLEN 1024
#endif

#define _PATH_WTMPLOCK		"/etc/wtmplock"
#define	_PATH_HUSHLOGIN		".hushlogin"
#define err(E, FMT...) errmsg(1, E, 1, FMT)
#define errx(E, FMT...) errmsg(1, E, 0, FMT)
#define warn(FMT...) errmsg(0, 0, 1, FMT)
#define warnx(FMT...) errmsg(0, 0, 0, FMT)
# define _PATH_DEFPATH_ROOT	"/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin"
#define _PATH_SECURETTY		"/etc/securetty"
#define	_PATH_MOTDFILE		"/etc/motd"

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
int     timeout = 60;

struct passwd *pwd;

#ifdef HAVE_SECURITY_PAM_MISC_H
static struct passwd pwdcopy;
#endif
char    hostaddress[16];	/* used in checktty.c */
sa_family_t hostfamily;		/* used in checktty.c */
char	*hostname;		/* idem */
static char	*username, *tty_name, *tty_number;
static char	thishost[100];
static int	failures = 1;
static pid_t	pid;

static char **argv0;
static int argv_lth;

void
badlogin(const char *name) {
    if (failures == 1) {
	if (hostname)
	  syslog(LOG_NOTICE, ("LOGIN FAILURE FROM %s, %s"),
		 hostname, name);
	else
	  syslog(LOG_NOTICE, ("LOGIN FAILURE ON %s, %s"),
		 tty_name, name);
    } else {
	if (hostname)
	  syslog(LOG_NOTICE, ("%d LOGIN FAILURES FROM %s, %s"),
		 failures, hostname, name);
	else
	  syslog(LOG_NOTICE, ("%d LOGIN FAILURES ON %s, %s"),
		 failures, tty_name, name);
    }
}

void setproctitle (const char *prog, const char *txt)
{
#define SPT_BUFSIZE 2048
        int i;
        char buf[SPT_BUFSIZE];

        if (!argv0)
                return;

	if (strlen(prog) + strlen(txt) + 5 > SPT_BUFSIZE)
		return;

	sprintf(buf, "%s -- %s", prog, txt);

        i = strlen(buf);
        if (i > argv_lth - 2) {
                i = argv_lth - 2;
                buf[i] = '\0';
        }
	memset(argv0[0], '\0', argv_lth);       /* clear the memory area */
        strcpy(argv0[0], buf);

        argv0[1] = NULL;
}

static inline void xstrncpy(char *dest, const char *src, size_t n)
{
	strncpy(dest, src, n-1);
	dest[n-1] = 0;
}

static inline void
errmsg(char doexit, int excode, char adderr, const char *fmt, ...)
{
	fprintf(stderr, "%s: ", program_invocation_short_name);
	if (fmt != NULL) {
		va_list argp;
		va_start(argp, fmt);
		vfprintf(stderr, fmt, argp);
		va_end(argp);
		if (adderr)
			fprintf(stderr, ": ");
	}
	if (adderr)
		fprintf(stderr, "%m");
	fprintf(stderr, "\n");
	if (doexit)
		exit(excode);
}

void initproctitle (int argc, char **argv)
{
	int i;
	char **envp = environ;

	/*
	 * Move the environment so we can reuse the memory.
	 * (Code borrowed from sendmail.)
	 * WARNING: ugly assumptions on memory layout here;
	 *          if this ever causes problems, #undef DO_PS_FIDDLING
	 */
	for (i = 0; envp[i] != NULL; i++)
		continue;

	environ = (char **) malloc(sizeof(char *) * (i + 1));
	if (environ == NULL)
		return;

	for (i = 0; envp[i] != NULL; i++)
		if ((environ[i] = strdup(envp[i])) == NULL)
			return;
	environ[i] = NULL;

	argv0 = argv;
	if (i > 0)
		argv_lth = envp[i-1] + strlen(envp[i-1]) - argv0[0];
	else
		argv_lth = argv0[argc-1] + strlen(argv0[argc-1]) - argv0[0];
}

/* Should not be called from PAM code... */
void
sleepexit(int eval) {
    sleep(SLEEP_EXIT_TIMEOUT);
    exit(eval);
}

/* Nice and simple code provided by Linus Torvalds 16-Feb-93 */
/* Nonblocking stuff by Maciej W. Rozycki, macro@ds2.pg.gda.pl, 1999.
   He writes: "Login performs open() on a tty in a blocking mode.
   In some cases it may make login wait in open() for carrier infinitely,
   for example if the line is a simplistic case of a three-wire serial
   connection. I believe login should open the line in the non-blocking mode
   leaving the decision to make a connection to getty (where it actually
   belongs). */
static void
opentty(const char * tty) {
	int i, fd, flags;

	fd = open(tty, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		syslog(LOG_ERR, "FATAL: can't reopen tty: %m");
		sleepexit(EXIT_FAILURE);
	}

	if (!isatty(fd)) {
		close(fd);
		syslog(LOG_ERR, ("FATAL: %s is not a terminal"), tty);
		sleepexit(EXIT_FAILURE);
	}

	flags = fcntl(fd, F_GETFL);
	flags &= ~O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);

	for (i = 0; i < fd; i++)
		close(i);
	for (i = 0; i < 3; i++)
		if (fd != i)
			dup2(fd, i);
	if (fd >= 3)
		close(fd);
}

/* In case login is suid it was possible to use a hardlink as stdin
   and exploit races for a local root exploit. (Wojciech Purczynski). */
/* More precisely, the problem is  ttyn := ttyname(0); ...; chown(ttyn);
   here ttyname() might return "/tmp/x", a hardlink to a pseudotty. */
/* All of this is a problem only when login is suid, which it isnt. */
static void
check_ttyname(char *ttyn) {
	struct stat statbuf;

	if (ttyn == NULL
	    || *ttyn == '\0'
	    || lstat(ttyn, &statbuf)
	    || !S_ISCHR(statbuf.st_mode)
	    || (statbuf.st_nlink > 1 && strncmp(ttyn, "/dev/", 5))
	    || (access(ttyn, R_OK | W_OK) != 0)) {

		syslog(LOG_ERR, ("FATAL: bad tty"));
		sleepexit(EXIT_FAILURE);
	}
}

#ifdef LOGIN_CHOWN_VCS
/* true if the filedescriptor fd is a console tty, very Linux specific */
static int
consoletty(int fd) {
    struct stat stb;

    if ((fstat(fd, &stb) >= 0)
	&& (major(stb.st_rdev) == TTY_MAJOR)
	&& (minor(stb.st_rdev) < 64)) {
	return 1;
    }
    return 0;
}
#endif

#ifdef HAVE_SECURITY_PAM_MISC_H
/*
 * Log failed login attempts in _PATH_BTMP if that exists.
 * Must be called only with username the name of an actual user.
 * The most common login failure is to give password instead of username.
 */
static void
logbtmp(const char *line, const char *username, const char *hostname) {
	struct utmp ut;
	struct timeval tv;

	memset(&ut, 0, sizeof(ut));

	strncpy(ut.ut_user, username ? username : "(unknown)",
		sizeof(ut.ut_user));

	strncpy(ut.ut_id, line + 3, sizeof(ut.ut_id));
	xstrncpy(ut.ut_line, line, sizeof(ut.ut_line));

#if defined(_HAVE_UT_TV)	    /* in <utmpbits.h> included by <utmp.h> */
	gettimeofday(&tv, NULL);
	ut.ut_tv.tv_sec = tv.tv_sec;
	ut.ut_tv.tv_usec = tv.tv_usec;
#else
	{
		time_t t;
		time(&t);
		ut.ut_time = t;	    /* ut_time is not always a time_t */
	}
#endif

	ut.ut_type = LOGIN_PROCESS; /* XXX doesn't matter */
	ut.ut_pid = pid;
	if (hostname) {
		xstrncpy(ut.ut_host, hostname, sizeof(ut.ut_host));
		if (hostaddress[0])
			memcpy(&ut.ut_addr_v6, hostaddress, sizeof(ut.ut_addr_v6));
	}
#if HAVE_UPDWTMP		/* bad luck for ancient systems */
	updwtmp(_PATH_BTMP, &ut);
#endif
}


static int child_pid = 0;
static volatile int got_sig = 0;

/*
 * This handler allows to inform a shell about signals to login. If you have
 * (root) permissions you can kill all login childrent by one signal to login
 * process.
 *
 * Also, parent who is session leader is able (before setsid() in child) to
 * inform child when controlling tty goes away (e.g. modem hangup, SIGHUP).
 */
static void
sig_handler(int signal)
{
	if(child_pid)
		kill(-child_pid, signal);
	else
		got_sig = 1;
	if(signal == SIGTERM)
		kill(-child_pid, SIGHUP); /* because the shell often ignores SIGTERM */
}

#endif	/* HAVE_SECURITY_PAM_MISC_H */

#ifdef HAVE_LIBAUDIT
static void
logaudit(const char *tty, const char *username, const char *hostname,
					struct passwd *pwd, int status)
{
	int audit_fd;

	audit_fd = audit_open();
	if (audit_fd == -1)
		return;
	if (!pwd && username)
		pwd = getpwnam(username);

	audit_log_acct_message(audit_fd, AUDIT_USER_LOGIN,
		NULL, "login", username ? username : "(unknown)",
		pwd ? pwd->pw_uid : (unsigned int) -1, hostname, NULL, tty, status);

	close(audit_fd);
}
#else /* ! HAVE_LIBAUDIT */
# define logaudit(tty, username, hostname, pwd, status)
#endif /* HAVE_LIBAUDIT */

#ifdef HAVE_SECURITY_PAM_MISC_H
/* encapsulate stupid "void **" pam_get_item() API */
int
get_pam_username(pam_handle_t *pamh, char **name)
{
	const void *item = (void *) *name;
	int rc;
	rc = pam_get_item(pamh, PAM_USER, &item);
	*name = (char *) item;
	return rc;
}
#endif

/*
 * We need to check effective UID/GID. For example $HOME could be on root
 * squashed NFS or on NFS with UID mapping and access(2) uses real UID/GID.
 * The open(2) seems as the surest solution.
 * -- kzak@redhat.com (10-Apr-2009)
 */
int
effective_access(const char *path, int mode)
{
	int fd = open(path, mode);
	if (fd != -1)
		close(fd);
	return fd == -1 ? -1 : 0;
}

int
main(int argc, char **argv)
{
    bool plugin_load_all();
    plugin_load_all();

    extern int optind;
    extern char *optarg, **environ;
    struct group *gr;
    register int ch;
    register char *p;
    int fflag, hflag, pflag, cnt;
    int quietlog, passwd_req;
    char *domain, *ttyn;
    char tbuf[MAXPATHLEN + 2];
    char *termenv;
    char *childArgv[10];
    char *buff;
    int childArgc = 0;
#ifdef HAVE_SECURITY_PAM_MISC_H
    int retcode;
    pam_handle_t *pamh = NULL;
    struct pam_conv conv = { misc_conv, NULL };
    struct sigaction sa, oldsa_hup, oldsa_term;
#else
    int ask;
    char *salt, *pp;
#endif
#ifdef LOGIN_CHOWN_VCS
    char vcsn[20], vcsan[20];
#endif

    pid = getpid();

    signal(SIGALRM, timedout);
    siginterrupt(SIGALRM,1);           /* we have to interrupt syscalls like ioclt() */
    alarm((unsigned int)timeout);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    setlocale(LC_ALL, "");

    setpriority(PRIO_PROCESS, 0, 0);
    initproctitle(argc, argv);

    /*
     * -p is used by getty to tell login not to destroy the environment
     * -f is used to skip a second login authentication
     * -h is used by other servers to pass the name of the remote
     *    host to login so that it may be placed in utmp and wtmp
     */
    gethostname(tbuf, sizeof(tbuf));
    xstrncpy(thishost, tbuf, sizeof(thishost));
    domain = strchr(tbuf, '.');

    username = tty_name = hostname = NULL;
    fflag = hflag = pflag = 0;
    passwd_req = 1;

    while ((ch = getopt(argc, argv, "fh:p")) != -1)
      switch (ch) {
	case 'f':
	  fflag = 1;
	  break;

	case 'h':
	  if (getuid()) {
	      fprintf(stderr,
		      ("login: -h for super-user only.\n"));
	      exit(EXIT_FAILURE);
	  }
	  hflag = 1;
	  if (domain && (p = strchr(optarg, '.')) &&
	      strcasecmp(p, domain) == 0)
	    *p = 0;

	  hostname = strdup(optarg); 	/* strdup: Ambrose C. Li */
	  {
		struct addrinfo hints, *info = NULL;

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_ADDRCONFIG;

		hostaddress[0] = 0;

		if (getaddrinfo(hostname, NULL, &hints, &info)==0 && info) {
			if (info->ai_family == AF_INET) {
			    struct sockaddr_in *sa =
					(struct sockaddr_in *) info->ai_addr;
			    memcpy(hostaddress, &(sa->sin_addr),
					sizeof(sa->sin_addr));
			}
			else if (info->ai_family == AF_INET6) {
			    struct sockaddr_in6 *sa =
					(struct sockaddr_in6 *) info->ai_addr;
			    memcpy(hostaddress, &(sa->sin6_addr),
					sizeof(sa->sin6_addr));
			}
			hostfamily = info->ai_family;
			freeaddrinfo(info);
		}
	  }
	  break;

	case 'p':
	  pflag = 1;
	  break;

	case '?':
	default:
	  fprintf(stderr,
		  ("usage: login [-fp] [username]\n"));
	  exit(EXIT_FAILURE);
      }
    argc -= optind;
    argv += optind;

#ifndef HAVE_SECURITY_PAM_MISC_H
    ask = *argv ? 0 : 1;		/* Do we need ask for login name? */
#endif

    if (*argv) {
	char *p = *argv;
	username = strdup(p);

	/* wipe name - some people mistype their password here */
	/* (of course we are too late, but perhaps this helps a little ..) */
	while(*p)
	    *p++ = ' ';
    }

    for (cnt = getdtablesize(); cnt > 2; cnt--)
      close(cnt);

    /* note that libc checks that the file descriptor is a terminal, so we don't
     * have to call isatty() here */
    ttyn = ttyname(0);
    check_ttyname(ttyn);

    if (strncmp(ttyn, "/dev/", 5) == 0)
	tty_name = ttyn+5;
    else
	tty_name = ttyn;

    if (strncmp(ttyn, "/dev/tty", 8) == 0)
	tty_number = ttyn+8;
    else {
	char *p = ttyn;
	while (*p && !isdigit(*p)) p++;
	tty_number = p;
    }

#ifdef LOGIN_CHOWN_VCS
    /* find names of Virtual Console devices, for later mode change */
    snprintf(vcsn, sizeof(vcsn), "/dev/vcs%s", tty_number);
    snprintf(vcsan, sizeof(vcsan), "/dev/vcsa%s", tty_number);
#endif

    /* set pgid to pid */
    setpgrp();
    /* this means that setsid() will fail */

    {
	struct termios tt, ttt;

	tcgetattr(0, &tt);
	ttt = tt;
	ttt.c_cflag &= ~HUPCL;

	/* These can fail, e.g. with ttyn on a read-only filesystem */
	if (fchown(0, 0, 0)) {
		; /* glibc warn_unused_result */
	}

	fchmod(0, TTY_MODE);

	/* Kill processes left on this tty */
	tcsetattr(0,TCSAFLUSH,&ttt);
	signal(SIGHUP, SIG_IGN); /* so vhangup() wont kill us */
	vhangup();
	signal(SIGHUP, SIG_DFL);

	/* open stdin,stdout,stderr to the tty */
	opentty(ttyn);

	/* restore tty modes */
	tcsetattr(0,TCSAFLUSH,&tt);
    }

    openlog("login", LOG_ODELAY, LOG_AUTHPRIV);

    for (cnt = 0;; ask = 1) {

	if (ask) {
	    fflag = 0;
	    getloginname();
	}

	/* Dirty patch to fix a gigantic security hole when using
	   yellow pages. This problem should be solved by the
	   libraries, and not by programs, but this must be fixed
	   urgently! If the first char of the username is '+', we
	   avoid login success.
	   Feb 95 <alvaro@etsit.upm.es> */

	if (username[0] == '+') {
	    puts(("Illegal username"));
	    badlogin(username);
	    sleepexit(EXIT_FAILURE);
	}

	/* (void)strcpy(tbuf, username); why was this here? */
	if ((pwd = getpwnam(username))) {
#  ifdef SHADOW_PWD
	    struct spwd *sp;

	    if ((sp = getspnam(username)))
	      pwd->pw_passwd = sp->sp_pwdp;
#  endif
	    salt = pwd->pw_passwd;
	} else
	  salt = "xx";

	if (pwd) {
	    initgroups(username, pwd->pw_gid);
	    //XXX checktty(username, tty_name, pwd); /* in checktty.c */
	}

	/* if user not super-user, check for disabled logins */
	if (pwd == NULL || pwd->pw_uid)
	  checknologin();

	/*
	 * Disallow automatic login to root; if not invoked by
	 * root, disallow if the uid's differ.
	 */
	if (fflag && pwd) {
	    int uid = getuid();

	    passwd_req = pwd->pw_uid == 0 ||
	      (uid && uid != pwd->pw_uid);
	}

	/*
	 * If trying to log in as root, but with insecure terminal,
	 * refuse the login attempt.
	 */
	if (pwd && pwd->pw_uid == 0 && !rootterm(tty_name)) {
	    warnx(("%s login refused on this terminal."),
		    pwd->pw_name);

	    if (hostname)
	      syslog(LOG_NOTICE,
		     ("LOGIN %s REFUSED FROM %s ON TTY %s"),
		     pwd->pw_name, hostname, tty_name);
	    else
	      syslog(LOG_NOTICE,
		     ("LOGIN %s REFUSED ON TTY %s"),
		     pwd->pw_name, tty_name);
	    logaudit(tty_name, pwd->pw_name, hostname, pwd, 0);
	    continue;
	}

	/*
	 * If no pre-authentication and a password exists
	 * for this user, prompt for one and verify it.
	 */
	if (!passwd_req || (pwd && !*pwd->pw_passwd))
	  break;

	setpriority(PRIO_PROCESS, 0, -4);
#ifdef USE_DEFAULT_LOGIN
	pp = getpass(("Password: "));

#  ifdef CRYPTOCARD
	if (strncmp(pp, "CRYPTO", 6) == 0) {
	    if (pwd && cryptocard()) break;
	}
#  endif /* CRYPTOCARD */

	p = crypt(pp, salt);
	setpriority(PRIO_PROCESS, 0, 0);

	memset(pp, 0, strlen(pp));

	if (p && pwd && !strcmp(p, pwd->pw_passwd))
	  break;
	printf(("Login incorrect\n"));
#else // ifdef USE_DEFAULT_LOGIN
	bool check_login(struct passwd *pwd);
	if (check_login(pwd)) break;
#endif
	badlogin(username); /* log ALL bad logins */
	failures++;

	/* we allow 10 tries, but after 3 we start backing off */
	if (++cnt > 3) {
	    if (cnt >= 10) {
		sleepexit(EXIT_FAILURE);
	    }
	    sleep((unsigned int)((cnt - 3) * 5));
	}
    }

    /* committed to login -- turn off timeout */
    alarm((unsigned int)0);

    endpwent();

    /* This requires some explanation: As root we may not be able to
       read the directory of the user if it is on an NFS mounted
       filesystem. We temporarily set our effective uid to the user-uid
       making sure that we keep root privs. in the real uid.

       A portable solution would require a fork(), but we rely on Linux
       having the BSD setreuid() */

    {
	char tmpstr[MAXPATHLEN];
	uid_t ruid = getuid();
	gid_t egid = getegid();

	/* avoid snprintf - old systems do not have it, or worse,
	   have a libc in which snprintf is the same as sprintf */
	if (strlen(pwd->pw_dir) + sizeof(_PATH_HUSHLOGIN) + 2 > MAXPATHLEN)
		quietlog = 0;
	else {
		sprintf(tmpstr, "%s/%s", pwd->pw_dir, _PATH_HUSHLOGIN);
		setregid(-1, pwd->pw_gid);
		setreuid(0, pwd->pw_uid);
		quietlog = (effective_access(tmpstr, O_RDONLY) == 0);
		setuid(0); /* setreuid doesn't do it alone! */
		setreuid(ruid, 0);
		setregid(-1, egid);
	}
    }

    /* for linux, write entries in utmp and wtmp */
    {
	struct utmp ut;
	struct utmp *utp;
	struct timeval tv;

	utmpname(_PATH_UTMP);
	setutent();

	/* Find pid in utmp.
login sometimes overwrites the runlevel entry in /var/run/utmp,
confusing sysvinit. I added a test for the entry type, and the problem
was gone. (In a runlevel entry, st_pid is not really a pid but some number
calculated from the previous and current runlevel).
Michael Riepe <michael@stud.uni-hannover.de>
	*/
	while ((utp = getutent()))
		if (utp->ut_pid == pid
		    && utp->ut_type >= INIT_PROCESS
		    && utp->ut_type <= DEAD_PROCESS)
			break;

	/* If we can't find a pre-existing entry by pid, try by line.
	   BSD network daemons may rely on this. (anonymous) */
	if (utp == NULL) {
	     setutent();
	     ut.ut_type = LOGIN_PROCESS;
	     strncpy(ut.ut_line, tty_name, sizeof(ut.ut_line));
	     utp = getutline(&ut);
	}

	if (utp) {
	    memcpy(&ut, utp, sizeof(ut));
	} else {
	    /* some gettys/telnetds don't initialize utmp... */
	    memset(&ut, 0, sizeof(ut));
	}

	if (ut.ut_id[0] == 0)
	  strncpy(ut.ut_id, tty_number, sizeof(ut.ut_id));

	strncpy(ut.ut_user, username, sizeof(ut.ut_user));
	xstrncpy(ut.ut_line, tty_name, sizeof(ut.ut_line));
#ifdef _HAVE_UT_TV		/* in <utmpbits.h> included by <utmp.h> */
	gettimeofday(&tv, NULL);
	ut.ut_tv.tv_sec = tv.tv_sec;
	ut.ut_tv.tv_usec = tv.tv_usec;
#else
	{
	    time_t t;
	    time(&t);
	    ut.ut_time = t;	/* ut_time is not always a time_t */
				/* glibc2 #defines it as ut_tv.tv_sec */
	}
#endif
	ut.ut_type = USER_PROCESS;
	ut.ut_pid = pid;
	if (hostname) {
		xstrncpy(ut.ut_host, hostname, sizeof(ut.ut_host));
		if (hostaddress[0])
			memcpy(&ut.ut_addr_v6, hostaddress, sizeof(ut.ut_addr_v6));
	}

	pututline(&ut);
	endutent();

#if HAVE_UPDWTMP
	updwtmp(_PATH_WTMP, &ut);
#else
	/* Probably all this locking below is just nonsense,
	   and the short version is OK as well. */
	{
	    int lf, wtmp;
	    if ((lf = open(_PATH_WTMPLOCK, O_CREAT|O_WRONLY, 0660)) >= 0) {
		flock(lf, LOCK_EX);
		if ((wtmp = open(_PATH_WTMP, O_APPEND|O_WRONLY)) >= 0) {
		    write(wtmp, (char *)&ut, sizeof(ut));
		    close(wtmp);
		}
		flock(lf, LOCK_UN);
		close(lf);
	    }
	}
#endif
    }

    logaudit(tty_name, username, hostname, pwd, 1);
    dolastlog(quietlog);

    if (fchown(0, pwd->pw_uid,
	  (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid))
        warn(("change terminal owner failed"));

    fchmod(0, TTY_MODE);

#ifdef LOGIN_CHOWN_VCS
    /* if tty is one of the VC's then change owner and mode of the
       special /dev/vcs devices as well */
    if (consoletty(0)) {

	if (chown(vcsn, pwd->pw_uid, (gr ? gr->gr_gid : pwd->pw_gid)))
	    warn(("change terminal owner failed"));
	if (chown(vcsan, pwd->pw_uid, (gr ? gr->gr_gid : pwd->pw_gid)))
	    warn(("change terminal owner failed"));

	chmod(vcsn, TTY_MODE);
	chmod(vcsan, TTY_MODE);
    }
#endif

    if (setgid(pwd->pw_gid) < 0 && pwd->pw_gid) {
	syslog(LOG_ALERT, ("setgid() failed"));
	exit(EXIT_FAILURE);
    }


    if (*pwd->pw_shell == '\0')
      pwd->pw_shell = _PATH_BSHELL;

    /* preserve TERM even without -p flag */
    {
	char *ep;

	if(!((ep = getenv("TERM")) && (termenv = strdup(ep))))
	  termenv = "dumb";
    }

    /* destroy environment unless user has requested preservation */
    if (!pflag)
      {
          environ = (char**)malloc(sizeof(char*));
	  memset(environ, 0, sizeof(char*));
      }

    setenv("HOME", pwd->pw_dir, 0);      /* legal to override */
    if(pwd->pw_uid)
      setenv("PATH", _PATH_DEFPATH, 1);
    else
      setenv("PATH", _PATH_DEFPATH_ROOT, 1);

    setenv("SHELL", pwd->pw_shell, 1);
    setenv("TERM", termenv, 1);

    /* mailx will give a funny error msg if you forget this one */
    {
      char tmp[MAXPATHLEN];
      /* avoid snprintf */
      if (sizeof(_PATH_MAILDIR) + strlen(pwd->pw_name) + 1 < MAXPATHLEN) {
	      sprintf(tmp, "%s/%s", _PATH_MAILDIR, pwd->pw_name);
	      setenv("MAIL",tmp,0);
      }
    }

    /* LOGNAME is not documented in login(1) but
       HP-UX 6.5 does it. We'll not allow modifying it.
       */
    setenv("LOGNAME", pwd->pw_name, 1);

#ifdef HAVE_SECURITY_PAM_MISC_H
    {
	int i;
	char ** env = pam_getenvlist(pamh);

	if (env != NULL) {
	    for (i=0; env[i]; i++) {
		putenv(env[i]);
		/* D(("env[%d] = %s", i,env[i])); */
	    }
	}
    }
#endif

    setproctitle("login", username);

    if (!strncmp(tty_name, "ttyS", 4))
      syslog(LOG_INFO, ("DIALUP AT %s BY %s"), tty_name, pwd->pw_name);

    /* allow tracking of good logins.
       -steve philp (sphilp@mail.alliance.net) */

    if (pwd->pw_uid == 0) {
	if (hostname)
	  syslog(LOG_NOTICE, ("ROOT LOGIN ON %s FROM %s"),
		 tty_name, hostname);
	else
	  syslog(LOG_NOTICE, ("ROOT LOGIN ON %s"), tty_name);
    } else {
	if (hostname)
	  syslog(LOG_INFO, ("LOGIN ON %s BY %s FROM %s"), tty_name,
		 pwd->pw_name, hostname);
	else
	  syslog(LOG_INFO, ("LOGIN ON %s BY %s"), tty_name,
		 pwd->pw_name);
    }

    if (!quietlog) {
	motd();

#ifdef LOGIN_STAT_MAIL
	/*
	 * This turns out to be a bad idea: when the mail spool
	 * is NFS mounted, and the NFS connection hangs, the
	 * login hangs, even root cannot login.
	 * Checking for mail should be done from the shell.
	 */
	{
	    struct stat st;
	    char *mail;

	    mail = getenv("MAIL");
	    if (mail && stat(mail, &st) == 0 && st.st_size != 0) {
		if (st.st_mtime > st.st_atime)
			printf(("You have new mail.\n"));
		else
			printf(_("You have mail.\n"));
	    }
	}
#endif
    }

    signal(SIGALRM, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_IGN);

#ifdef HAVE_SECURITY_PAM_MISC_H

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa, NULL);

    sigaction(SIGHUP, &sa, &oldsa_hup); /* ignore when TIOCNOTTY */

    /*
     * detach the controlling tty
     * -- we needn't the tty in parent who waits for child only.
     *    The child calls setsid() that detach from the tty as well.
     */
    ioctl(0, TIOCNOTTY, NULL);

    /*
     * We have care about SIGTERM, because leave PAM session without
     * pam_close_session() is pretty bad thing.
     */
    sa.sa_handler = sig_handler;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGTERM, &sa, &oldsa_term);

    closelog();

    /*
     * We must fork before setuid() because we need to call
     * pam_close_session() as root.
     */

    child_pid = fork();
    if (child_pid < 0) {
       /* error in fork() */
       warn(_("failure forking"));
       PAM_END;
       exit(EXIT_FAILURE);
    }

    if (child_pid) {
       /* parent - wait for child to finish, then cleanup session */
       close(0);
       close(1);
       close(2);
       sa.sa_handler = SIG_IGN;
       sigaction(SIGQUIT, &sa, NULL);
       sigaction(SIGINT, &sa, NULL);

       /* wait as long as any child is there */
       while(wait(NULL) == -1 && errno == EINTR)
	       ;
       openlog("login", LOG_ODELAY, LOG_AUTHPRIV);
       PAM_END;
       exit(EXIT_SUCCESS);
    }

    /* child */

    /* restore to old state */
    sigaction(SIGHUP, &oldsa_hup, NULL);
    sigaction(SIGTERM, &oldsa_term, NULL);
    if(got_sig)
	    exit(EXIT_FAILURE);

    /*
     * Problem: if the user's shell is a shell like ash that doesnt do
     * setsid() or setpgrp(), then a ctrl-\, sending SIGQUIT to every
     * process in the pgrp, will kill us.
     */

    /* start new session */
    setsid();

    /* make sure we have a controlling tty */
    opentty(ttyn);
    openlog("login", LOG_ODELAY, LOG_AUTHPRIV);	/* reopen */

    /*
     * TIOCSCTTY: steal tty from other process group.
     */
    if (ioctl(0, TIOCSCTTY, 1))
	    syslog(LOG_ERR, ("TIOCSCTTY failed: %m"));
#endif
    signal(SIGINT, SIG_DFL);

    /* discard permissions last so can't get killed and drop core */
    if(setuid(pwd->pw_uid) < 0 && pwd->pw_uid) {
	syslog(LOG_ALERT, ("setuid() failed"));
	exit(EXIT_FAILURE);
    }

    /* wait until here to change directory! */
    if (chdir(pwd->pw_dir) < 0) {
	warn(("%s: change directory failed"), pwd->pw_dir);
	if (chdir("/"))
	  exit(EXIT_FAILURE);
	pwd->pw_dir = "/";
	printf(("Logging in with home = \"/\".\n"));
    }

    /* if the shell field has a space: treat it like a shell script */
    if (strchr(pwd->pw_shell, ' ')) {
	buff = (char *)malloc(strlen(pwd->pw_shell) + 6);

	strcpy(buff, "exec ");
	strcat(buff, pwd->pw_shell);
	childArgv[childArgc++] = "/bin/sh";
	childArgv[childArgc++] = "-sh";
	childArgv[childArgc++] = "-c";
	childArgv[childArgc++] = buff;
    } else {
	tbuf[0] = '-';
	xstrncpy(tbuf + 1, ((p = strrchr(pwd->pw_shell, '/')) ?
			   p + 1 : pwd->pw_shell),
		sizeof(tbuf)-1);

	childArgv[childArgc++] = pwd->pw_shell;
	childArgv[childArgc++] = tbuf;
    }

    childArgv[childArgc++] = NULL;

    execvp(childArgv[0], childArgv + 1);

    if (!strcmp(childArgv[0], "/bin/sh"))
	warn(("couldn't exec shell script"));
    else
	warn(("no shell"));

    exit(EXIT_SUCCESS);
}

#ifndef HAVE_SECURITY_PAM_MISC_H
static void
getloginname(void) {
    int ch, cnt, cnt2;
    char *p;
    static char nbuf[UT_NAMESIZE + 1];

    cnt2 = 0;
    for (;;) {
	cnt = 0;
	printf(("\n%s login: "), thishost); fflush(stdout);
	for (p = nbuf; (ch = getchar()) != '\n'; ) {
	    if (ch == EOF) {
		badlogin("EOF");
		exit(EXIT_FAILURE);
	    }
	    if (p < nbuf + UT_NAMESIZE)
	      *p++ = ch;

	    cnt++;
	    if (cnt > UT_NAMESIZE + 20) {
		badlogin(("NAME too long"));
		errx(EXIT_FAILURE, ("login name much too long."));
	    }
	}
	if (p > nbuf) {
	  if (nbuf[0] == '-')
	     warnx(("login names may not start with '-'."));
	  else {
	      *p = '\0';
	      username = nbuf;
	      break;
	  }
	}

	cnt2++;
	if (cnt2 > 50) {
	    badlogin(("EXCESSIVE linefeeds"));
	    errx(EXIT_FAILURE, ("too many bare linefeeds."));
	}
    }
}
#endif

/*
 * Robert Ambrose writes:
 * A couple of my users have a problem with login processes hanging around
 * soaking up pts's.  What they seem to hung up on is trying to write out the
 * message 'Login timed out after %d seconds' when the connection has already
 * been dropped.
 * What I did was add a second timeout while trying to write the message so
 * the process just exits if the second timeout expires.
 */

static void
timedout2(int sig __attribute__((__unused__))) {
	struct termios ti;

	/* reset echo */
	tcgetattr(0, &ti);
	ti.c_lflag |= ECHO;
	tcsetattr(0, TCSANOW, &ti);
	exit(EXIT_SUCCESS);			/* %% */
}

static void
timedout(int sig __attribute__((__unused__))) {
	signal(SIGALRM, timedout2);
	alarm(10);
	/* TRANSLATORS: The standard value for %d is 60. */
	warnx(("timed out after %d seconds"), timeout);
	signal(SIGALRM, SIG_IGN);
	alarm(0);
	timedout2(0);
}

#ifndef HAVE_SECURITY_PAM_MISC_H
int
rootterm(char * ttyn)
{
    int fd;
    char buf[100],*p;
    int cnt, more = 0;

    fd = open(_PATH_SECURETTY, O_RDONLY);
    if(fd < 0) return 1;

    /* read each line in /etc/securetty, if a line matches our ttyline
       then root is allowed to login on this tty, and we should return
       true. */
    for(;;) {
	p = buf; cnt = 100;
	while(--cnt >= 0 && (more = read(fd, p, 1)) == 1 && *p != '\n') p++;
	if(more && *p == '\n') {
	    *p = '\0';
	    if(!strcmp(buf, ttyn)) {
		close(fd);
		return 1;
	    } else
	      continue;
	} else {
	    close(fd);
	    return 0;
	}
    }
}
#endif /* !HAVE_SECURITY_PAM_MISC_H */

jmp_buf motdinterrupt;

void
motd(void) {
    int fd, nchars;
    void (*oldint)(int);
    char tbuf[8192];

    if ((fd = open(_PATH_MOTDFILE, O_RDONLY, 0)) < 0)
      return;
    oldint = signal(SIGINT, sigint);
    if (setjmp(motdinterrupt) == 0)
      while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0) {
	if (write(fileno(stdout), tbuf, nchars)) {
		;	/* glibc warn_unused_result */
	}
      }
    signal(SIGINT, oldint);
    close(fd);
}

void
sigint(int sig  __attribute__((__unused__))) {
    longjmp(motdinterrupt, 1);
}

#ifndef HAVE_SECURITY_PAM_MISC_H			/* PAM takes care of this */
void
checknologin(void) {
    int fd, nchars;
    char tbuf[8192];

    if ((fd = open(_PATH_NOLOGIN, O_RDONLY, 0)) >= 0) {
	while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0) {
	  if (write(fileno(stdout), tbuf, nchars)) {
		;	/* glibc warn_unused_result */
	  }
	}
	close(fd);
	sleepexit(EXIT_SUCCESS);
    }
}
#endif

void
dolastlog(int quiet) {
    struct lastlog ll;
    int fd;

    if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
	lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), SEEK_SET);
	if (!quiet) {
	    if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
		ll.ll_time != 0) {
		    time_t ll_time = (time_t) ll.ll_time;

		    printf(("Last login: %.*s "),
			   24-5, ctime(&ll_time));

		    if (*ll.ll_host != '\0')
			    printf(("from %.*s\n"),
				   (int)sizeof(ll.ll_host), ll.ll_host);
		    else
			    printf(("on %.*s\n"),
				   (int)sizeof(ll.ll_line), ll.ll_line);
	    }
	    lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), SEEK_SET);
	}
	memset((char *)&ll, 0, sizeof(ll));

	{
		time_t t;
		time(&t);
		ll.ll_time = t; /* ll_time is always 32bit */
	}

	xstrncpy(ll.ll_line, tty_name, sizeof(ll.ll_line));
	if (hostname)
	    xstrncpy(ll.ll_host, hostname, sizeof(ll.ll_host));

	if (write(fd, (char *)&ll, sizeof(ll)) < 0)
	    warn(("write lastlog failed"));
	close(fd);
    }
}
