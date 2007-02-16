/* ccdv.c */
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define SETCOLOR_SUCCESS	(gANSIEscapes ? "\033\1331;32m" : "")
#define SETCOLOR_FAILURE	(gANSIEscapes ? "\033\1331;31m" : "")
#define SETCOLOR_WARNING	(gANSIEscapes ? "\033\1331;33m" : "")
#define SETCOLOR_COMMAND        (gANSIEscapes ? "\033\1331;35m" : "")
#define SETCOLOR_ERROROUTPUT    (gANSIEscapes ? "\033\1331;31m" : "")
#define SETCOLOR_WARNINGOUTPUT  (gANSIEscapes ? "\033\1331;39m" : "")
#define SETCOLOR_NORMAL		(gANSIEscapes ? "\033\1330;39m" : "")

#define TEXT_BLOCK_SIZE 8192
#define INDENT 2

size_t gNBufUsed = 0, gNBufAllocated = 0;
char *gBuf = NULL;
int gCCPID;
char gAction[64] = "";
char gTarget[64] = "";
char gAr[32] = "";
char gArLibraryTarget[64] = "";
int gDumpCmdArgs = 0;
char gArgsStr[32768];
int gColumns = 80;
int gANSIEscapes = 0;
int gExitStatus = 95;

static void DumpFormattedOutput(void)
{
	char *cp;
	char spaces[8 + 1] = "        ";
	char *saved;
	int curcol;
	int i;

	curcol = 0;
	saved = NULL;
	if(gDumpCmdArgs)
	  {
	    printf("%s%s%s", SETCOLOR_COMMAND, gArgsStr, SETCOLOR_NORMAL);
	    for(i = 0; i < gColumns; ++i)
	      {
		putchar('=');
	      }
	  }
	printf("%s%s%s", gDumpCmdArgs ? SETCOLOR_ERROROUTPUT : SETCOLOR_WARNINGOUTPUT,
	       gBuf + strlen(gArgsStr), SETCOLOR_NORMAL);
	free(gBuf);
}	/* DumpFormattedOutput */

/* Difftime(), only for timeval structures.  */
static void TimeValSubtract(struct timeval *tdiff, struct timeval *t1,
							struct timeval *t0)
{
	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if(tdiff->tv_usec < 0)
	{
		tdiff->tv_sec--;
		tdiff->tv_usec += 1000000;
	}
}	/* TimeValSubtract */

static void Wait(void)
{
	int pid2, status;

	do
	{
		status = 0;
		pid2 = (int) waitpid(gCCPID, &status, 0);
	}
	while(((pid2 >= 0) && (!WIFEXITED(status)))
		  || ((pid2 < 0) && (errno == EINTR)));
	if(WIFEXITED(status))
		gExitStatus = WEXITSTATUS(status);
}	/* Wait */

static int SlurpProgress(int fd)
{
	char s1[71];
	char *newbuf;
	int nready;
	size_t ntoread;
	ssize_t nread;
	struct timeval now, tnext, tleft;
	fd_set ss;
	fd_set ss2;
	const char *trail = "/-\\|", *trailcp;

	trailcp = trail;
	snprintf(s1, sizeof(s1), "%s%s%s... ", gAction, gTarget[0] ? " " : "",
			 gTarget);
	printf("\r%-70s%-9s", s1, "");
	fflush(stdout);

	gettimeofday(&now, NULL);
	tnext = now;
	tnext.tv_sec++;
	tleft.tv_sec = 1;
	tleft.tv_usec = 0;
	FD_ZERO(&ss2);
	FD_SET(fd, &ss2);
	for(;;)
	{
		if(gNBufUsed == (gNBufAllocated - 1))
		{
			if((newbuf =
				(char *) realloc(gBuf,
								 gNBufAllocated + TEXT_BLOCK_SIZE)) == NULL)
			{
				perror("ccdv: realloc");
				return (-1);
			}
			gNBufAllocated += TEXT_BLOCK_SIZE;
			gBuf = newbuf;
		}
		for(;;)
		{
			ss = ss2;
			nready = select(fd + 1, &ss, NULL, NULL, &tleft);
			if(nready == 1)
				break;
			if(nready < 0)
			{
				if(errno != EINTR)
				{
					perror("ccdv: select");
					return (-1);
				}
				continue;
			}
			gettimeofday(&now, NULL);
			if((now.tv_sec > tnext.tv_sec)
			   || ((now.tv_sec == tnext.tv_sec)
				   && (now.tv_usec >= tnext.tv_usec)))
			{
				tnext = now;
				tnext.tv_sec++;
				tleft.tv_sec = 1;
				tleft.tv_usec = 0;
				printf("\r%-71s%c%-7s", s1, *trailcp, "");
				fflush(stdout);
				if(*++trailcp == '\0')
					trailcp = trail;
			}
			else
			{
				TimeValSubtract(&tleft, &tnext, &now);
			}
		}
		ntoread = (gNBufAllocated - gNBufUsed - 1);
		nread = read(fd, gBuf + gNBufUsed, ntoread);
		if(nread < 0)
		{
			if(errno == EINTR)
				continue;
			perror("ccdv: read");
			return (-1);
		}
		else if(nread == 0)
		{
			break;
		}
		gNBufUsed += nread;
		gBuf[gNBufUsed] = '\0';
	}
	snprintf(s1, sizeof(s1), "%s%s%s: ", gAction, gTarget[0] ? " " : "",
			 gTarget);
	Wait();
	if(gExitStatus == 0)
	{
		printf("\r%-70s", s1);
		printf("[%s%s%s]",
			   ((gNBufUsed - strlen(gArgsStr)) <
				4) ? SETCOLOR_SUCCESS : SETCOLOR_WARNING, "OK",
			   SETCOLOR_NORMAL);
		printf("%-5s\n", " ");
	}
	else
	{
		printf("\r%-70s", s1);
		printf("[%s%s%s]", SETCOLOR_FAILURE, "ERROR", SETCOLOR_NORMAL);
		printf("%-2s\n", " ");
		gDumpCmdArgs = 1;	/* print cmd when there are errors */
	}
	fflush(stdout);
	return (0);
}	/* SlurpProgress */

static int SlurpAll(int fd)
{
	char *newbuf;
	size_t ntoread;
	ssize_t nread;

	printf("%s%s%s.\n", gAction, gTarget[0] ? " " : "", gTarget);
	fflush(stdout);

	for(;;)
	{
		if(gNBufUsed == (gNBufAllocated - 1))
		{
			if((newbuf =
				(char *) realloc(gBuf,
								 gNBufAllocated + TEXT_BLOCK_SIZE)) == NULL)
			{
				perror("ccdv: realloc");
				return (-1);
			}
			gNBufAllocated += TEXT_BLOCK_SIZE;
			gBuf = newbuf;
		}
		ntoread = (gNBufAllocated - gNBufUsed - 1);
		nread = read(fd, gBuf + gNBufUsed, ntoread);
		if(nread < 0)
		{
			if(errno == EINTR)
				continue;
			perror("ccdv: read");
			return (-1);
		}
		else if(nread == 0)
		{
			break;
		}
		gNBufUsed += nread;
		gBuf[gNBufUsed] = '\0';
	}
	Wait();
	gDumpCmdArgs = (gExitStatus != 0);	/* print cmd when there are errors */
	return (0);
}	/* SlurpAll */

static const char *Basename(const char *path)
{
	const char *cp;
	cp = strrchr(path, '/');
	if(cp == NULL)
		return (path);
	return (cp + 1);
}	/* Basename */

static const char *Extension(const char *path)
{
	const char *cp = path;
	cp = strrchr(path, '.');
	if(cp == NULL)
		return ("");
	return (cp);
}	/* Extension */

static void Usage(void)
{
	char *sUsage = "\
Usage: ccdv /path/to/cc CFLAGS...\n\
\n\
I wrote this to reduce the deluge Make output to make finding actual problems\n\
easier.  It is intended to be invoked from Makefiles, like this.  Instead of:\n\
\n\
	.c.o:\n\
		$(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $< -c\n\
\n\
Rewrite your rule so it looks like:\n\
\n\
	.c.o:\n\
		@ccdv $(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $< -c\n\
	.cpp.o:\n\
		@ccdv $(CXX) $(CFLAGS) $(DEFS) $(CPPFLAGS) $< -c\n\
\n\
ccdv 1.1.0 is Free under the GNU Public License.  Enjoy!\n\
  -- Mike Gleason, NcFTP Software <http://www.ncftp.com>\n\
  -- John F Meinel Jr, <http://ccdv.sourceforge.net>\n\
  ";
	fprintf(stderr, sUsage);
	exit(96);
}	/* Usage */

int main(int argc, char **argv)
{
	int pipe1[2];
	int devnull;
	char emerg[256];
	int fd;
	int nread;
	int i;
	int cc = 0;
	int yy = 0;
	int gcc = 0;
	int lemon = 0;
	const char *quote;

	if(argc < 2)
		Usage();

	snprintf(gAction, sizeof(gAction), "Running %s", Basename(argv[1]));
	memset(gArgsStr, 0, sizeof(gArgsStr));

	if(strcmp(gAction+8, "ar") == 0)
	  {
	    snprintf(gTarget, sizeof(gTarget), "%s", Basename(argv[i + 1]));
	  }
	else if(strcmp(gAction+8, "cc") == 0 ||
		strcmp(gAction+8, "ld") == 0 ||
		strcmp(gAction+8, "gcc") == 0 ||
		strcmp(gAction+8, "g++") == 0 ||
		strcmp(gAction+8, "c++") == 0)
	  {
	    gcc = 1;
	  }
	else if(strcmp(gAction+8, "lemon") == 0)
	  {
	    lemon = 1;
	  }

	for(i = 1; i < argc; i++)
	{
		quote = (strchr(argv[i], ' ') != NULL) ? "\"" : "";
		snprintf(gArgsStr + strlen(gArgsStr),
				 sizeof(gArgsStr) - strlen(gArgsStr), "%s%s%s%s%s",
				 (i == 1) ? "" : " ", quote, argv[i], quote,
				 (i == (argc - 1)) ? "\n" : "");
		if(gcc && (strcmp(argv[i], "-o") == 0) && ((i + 1) < argc))
		{
			if(strcasecmp(Extension(argv[i + 1]), ".o") != 0)
			{
				strcpy(gAction, "Linking");
				snprintf(gTarget, sizeof(gTarget), "%s",
						 Basename(argv[i + 1]));
			}
		}
		else if(strchr("-+/", (int) argv[i][0]) != NULL)
		{
			continue;
		}
		else if(strncasecmp(Extension(argv[i]), ".c", 2) == 0)
		{
			cc++;
			snprintf(gTarget, sizeof(gTarget), "%s", Basename(argv[i]));
		}
		else if(strncasecmp(Extension(argv[i]), ".y", 2) == 0)
		  {
		    if(lemon)
		      {
			strcpy(gAction, "Generating");
			snprintf(gTarget, sizeof(gTarget), "%s", Basename(argv[i]));
		      }
		    yy++;
		  }
		else if(strncasecmp(Extension(argv[i]), ".nas", 3) == 0 ||
			strncasecmp(Extension(argv[i]), ".asm", 3) == 0 ||
			strcasecmp(Extension(argv[i]), ".s") == 0)
		  {
		    strcpy(gAction, "Assembling");
		    snprintf(gTarget, sizeof(gTarget), "%s", Basename(argv[i]));
		}
		else if((i == 1) && (strcmp(Basename(argv[i]), "ar") == 0))
		{
			snprintf(gAr, sizeof(gAr), "%s", Basename(argv[i]));
		}
		else if((gArLibraryTarget[0] == '\0')
				&& (strcasecmp(Extension(argv[i]), ".a") == 0))
		{
			snprintf(gArLibraryTarget, sizeof(gArLibraryTarget), "%s",
					 Basename(argv[i]));
		}
	}
	if((gAr[0] != '\0') && (gArLibraryTarget[0] != '\0'))
	{
		strcpy(gAction, "Creating library");
		snprintf(gTarget, sizeof(gTarget), "%s", gArLibraryTarget);
	}
	else if(cc > 0)
	{
		strcpy(gAction, yy == 0 ? "Compiling" : "Generating");
	}

	if(pipe(pipe1) < 0)
	{
		perror("ccdv: pipe");
		exit(97);
	}

	(void) close(0);
	devnull = open("/dev/null", O_RDWR, 00666);
	if((devnull != 0) && (dup2(devnull, 0) == 0))
		close(devnull);

	gCCPID = (int) fork();
	if(gCCPID < 0)
	{
		(void) close(pipe1[0]);
		(void) close(pipe1[1]);
		perror("ccdv: fork");
		exit(98);
	}
	else if(gCCPID == 0)
	{
		/* Child */
		(void) close(pipe1[0]);	/* close read end */
		if(pipe1[1] != 1)
		{	/* use write end on stdout */
			(void) dup2(pipe1[1], 1);
			(void) close(pipe1[1]);
		}
		(void) dup2(1, 2);	/* use write end on stderr */
		execvp(argv[1], argv + 1);
		perror(argv[1]);
		exit(99);
	}

	/* parent */
	(void) close(pipe1[1]);	/* close write end */
	fd = pipe1[0];	/* use read end */

	gColumns = (getenv("COLUMNS") != NULL) ? atoi(getenv("COLUMNS")) : 80;
	gANSIEscapes = (getenv("TERM") != NULL)
		&&
		(strstr
		 ("vt100:vt102:vt220:vt320:xterm:ansi:linux:scoterm:scoansi:dtterm:cons25:cygwin",
		  getenv("TERM")) != NULL);
	gBuf = (char *) malloc(TEXT_BLOCK_SIZE);
	if(gBuf == NULL)
		goto panic;
	gNBufUsed = 0;
	gNBufAllocated = TEXT_BLOCK_SIZE;
	if(strlen(gArgsStr) < (gNBufAllocated - 1))
	{
		strcpy(gBuf, gArgsStr);
		gNBufUsed = strlen(gArgsStr);
	}

	if(isatty(1))
	{
		if(SlurpProgress(fd) < 0)
			goto panic;
	}
	else
	{
		if(SlurpAll(fd) < 0)
			goto panic;
	}
	DumpFormattedOutput();
	exit(gExitStatus);

  panic:
	gDumpCmdArgs = 1;	/* print cmd when there are errors */
	DumpFormattedOutput();
	while((nread = read(fd, emerg, (size_t) sizeof(emerg))) > 0)
		(void) write(2, emerg, (size_t) nread);
	Wait();
	exit(gExitStatus);
}	/* main */

/* eof ccdv.c */
