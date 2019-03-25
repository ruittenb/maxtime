/*
 ****************************************************************************
 * Name:         maxtime
 * Version:      0.36
 * Author:       Rene Uittenbogaard
 * Date:         2004-07-16
 * Last Changes: 2019-03-22
 * License:      This program is free software.
 *               It is distributed without any warranty, even without
 *               the implied warranties of merchantability or fitness
 *               for a particular purpose.
 *               You can redistribute it and/or modify it under the terms
 *               described by the GNU General Public License version 2.
 * Usage:        maxtime [-g] [-k] [-v] [-w] [--] <time> <command> [<arg1> ...]
 * Compilation:  gcc -o maxtime maxtime.c
 * Description:  Runs external command, but time out after <time> seconds.
 *               Returns its exit code, if possible.
 */
 
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#define ARGV0		"maxtime"
#define WAITSIGINTERVAL	100000

#define ERR_ARGC	101
#define ERR_ARGS	102
#define ERR_SIGACT	103
#define ERR_FORK	104
#define ERR_EXEC	105
#define ERR_SIGNALED	106
#define ERR_MALLOC	107
#define ERR_FPCONV	108
#define ERR_FPVAL	109
#define ERR_TIMER	110

// #define DEBUG	1

static const char id[] = "@(#) " ARGV0 " 2019-03-22 version 0.36";

struct itimerval *atimer, *oatimer;
float period;
pid_t cpid;
int *cstatus;
int opt_g = 0;
int opt_k = 0;
int opt_v = 0;
int opt_w = 0;
int killed = 0;

/*
 ****************************************************************************
 * signal handler (ALRM)
 */
void cexpired(int signal) {
	int convicts;
	
	setitimer(ITIMER_REAL, oatimer, NULL);
	killed = 1;
	if (opt_g) convicts = -cpid;
	else	   convicts = cpid;
	if (opt_v) fprintf(stderr, ARGV0 ": caught SIGALRM, killing child process %s%d\n",
		(opt_g ? "group " : ""), cpid);
	if (opt_v) fprintf(stderr, ARGV0 ": sending TERM\n");
	if (!kill(convicts, SIGTERM) && opt_k) {
		usleep(WAITSIGINTERVAL);
		if (opt_v) fprintf(stderr, ARGV0 ": sending KILL\n");
		kill(convicts, SIGKILL);
	}
	if (opt_w) {
		if (opt_v) fprintf(stderr, ARGV0 ": waiting for child to finish..\n");
		wait(cstatus);
	}
}

void usage() {
	fprintf(stderr, "Usage: " ARGV0 " [-g] [-k] [-v] [-w] [--] <time> <command> [ <arg1> ... ]\n");
}

/*
 ****************************************************************************
 * main
 */
int main(int argc, char **argv) {
	extern char **environ;
	extern char *optarg;
	extern int optind, opterr, optopt;
	extern int errno;
	
	struct sigaction *alrm_act, *chld_act;
	int option;
	const char *optstring = "+gkvw"; // the + means: do not permute args
	
	while ((option = getopt(argc, argv, optstring)) > 0) {
		switch (option) {
			case 'g': opt_g = 1; break;
			case 'k': opt_k = 1; break;
			case 'v': opt_v = 1; break;
			case 'w': opt_w = 1; break;
			default : usage();   exit(ERR_ARGS);
		}
	}
	if (argc < optind+2) {
		usage();
		exit(ERR_ARGC);
	}
	if (!sscanf(argv[optind], "%f", &period)) {
		if (opt_v) fprintf(stderr, ARGV0 ": error: alarm period is not a valid number\n");
		exit(ERR_FPCONV);
	}
#ifdef DEBUG
	printf("opts gkvw: %d%d%d%d optind: %d period: %f\n", opt_g, opt_k, opt_v, opt_w, optind, period);
#endif
	if (period <= 0) {
		if (opt_v) fprintf(stderr, ARGV0 ": error: alarm period must be positive\n");
		exit(ERR_FPVAL);
	}
	argv += optind + 1;
	argc -= optind + 1;

	/* set SIGCHLD handler to default NOW:
	 * it seems that there are situations in which for user 'root' on HP systems,
	 * the default action is to ignore the SIGCHLD signal. This is not what we want.
	 */
	if ( !(chld_act = malloc(sizeof(struct sigaction))) ) {
		if (opt_v) perror(ARGV0 ": error: cannot malloc() for alarm handler");
		exit(ERR_MALLOC);
	}
	chld_act->sa_sigaction = NULL;
	chld_act->sa_flags     = 0;
	chld_act->sa_handler   = SIG_DFL;
	if (sigemptyset( &chld_act->sa_mask) && opt_v)
		fprintf(stderr, ARGV0 ": warning: could not empty sigactionmask\n");
	if (sigaction(SIGCHLD, chld_act, NULL) < 0) {
		if (opt_v) perror(ARGV0 ": error: cannot install sigchild handler");
		exit(ERR_SIGACT);
	}

	// now fork
	if ((cpid = fork()) < 0) {
		// fork failed
		if (opt_v) perror(ARGV0 ": error: cannot fork()");
		exit(ERR_FORK);
	} else if (!cpid) {
		// child
		if (opt_g)
//			if (setpgrp())
			if (setpgid(0,0)) // works better on HP-UX
				if (opt_v) perror(ARGV0 ": warning: could not start new process group");
		execvp(argv[0], argv);
		if (opt_v) fprintf(stderr, ARGV0 ": error: exec(%s) failed\n", argv[0]);
		exit(ERR_EXEC);
	} else {
		// parent
		if ( !(alrm_act = malloc(sizeof(struct sigaction))) ) {
			if (opt_v) perror(ARGV0 ": error: cannot malloc() for alarm handler");
			exit(ERR_MALLOC);
		}
		alrm_act->sa_sigaction = NULL;
		alrm_act->sa_flags     = 0;
		alrm_act->sa_handler   = cexpired;
		if (sigemptyset( &alrm_act->sa_mask) && opt_v)
			fprintf(stderr, ARGV0 ": warning: could not empty sigactionmask\n");
		if (sigaction(SIGALRM, alrm_act, NULL) < 0) {
			if (opt_v) perror(ARGV0 ": error: cannot install alarm handler");
			exit(ERR_SIGACT);
		}
		if ( !(cstatus = malloc(sizeof(int))) ) {
			if (opt_v) perror(ARGV0 ": error: cannot malloc() for exit status");
			exit(ERR_MALLOC);
		}
		if ( !(atimer  = malloc(sizeof(struct itimerval)))
		||   !(oatimer = malloc(sizeof(struct itimerval)))
		) {
			if (opt_v) perror(ARGV0 ": error: cannot malloc() for timer data");
			exit(ERR_MALLOC);
		}
		atimer->it_interval.tv_sec  = (long) period;
		atimer->it_interval.tv_usec = (long) ((period - (long)period) * 1000000);
		atimer->it_value = atimer->it_interval;
#ifdef DEBUG
		printf("split %f into: %ld s + %ld us\n", period, atimer->it_interval.tv_sec, atimer->it_interval.tv_usec);
#endif
		if (opt_v) fprintf(stderr, ARGV0 ": setting timer to %3.1f seconds\n", period);
		if (setitimer(ITIMER_REAL, atimer, oatimer)) {
			if (opt_v) perror(ARGV0 ": error: cannot set timer");
			exit(ERR_TIMER);
		}
		wait(cstatus);
		if (WIFEXITED(*cstatus) && !killed) {
			if (opt_v) fprintf(stderr, ARGV0 ": child exited with status %d\n", WEXITSTATUS(*cstatus));
			exit(WEXITSTATUS(*cstatus));
		} else {
			if (opt_v) fprintf(stderr, ARGV0 ": child was killed\n");
			exit(ERR_SIGNALED);
		}
	}
}

/*
 ***************************************************************************
 * pod documentation

=pod

=for section 1

=head1 NAME

C<maxtime> - run a command with a timeout

=head1 SYNOPSIS

C<maxtime [-g] [-k] [-v] [-w] [--] >I<time>C< >I<command>C< [>I<arg1>C< ...]>

=head1 DESCRIPTION

B<maxtime> allows for an external program to be run with a time limit.
The argument I<time> must be a positive number of seconds, and may be
fractional. If the I<command> terminates within this period, B<maxtime>
reports its exit status. If the command's real (wall clock)
time exceeds
the time limit, it will be killed with SIGTERM. B<maxtime> will then return
the value 106 to indicate the timeout (see below under RETURN VALUE).

=head1 OPTIONS

=over 4

=item B<-g>

The specified I<command> will run in its own process group. If the command
times out, the entire process group will be killed.

=item B<-k>

If SIGTERM does not kill I<command>, send SIGKILL after a short delay.

=item B<-v>

Be verbose. If not specified, B<maxtime> will not report what it is doing,
even if it exits due to an error. However, the exit code will indicate
the problem.

=item B<-w>

Wait for termination of the child process, even if it timed out and was
sent a SIGTERM and/or SIGKILL.

=back

=head1 ENVIRONMENT

=over 4

=item PATH

B<PATH> is traversed in order to find the specified I<command>.

=back

=head1 RETURN VALUE

B<maxtime> returns the exit status of the external command, unless an
error condition occurs, in which case the exit code will be greater than 100.

=over 4

=item Z<>0

The command completed successfully.

=item Z<>1 to 100

The command was unsuccessful. The exit status is the exit status
of the external command.

=item Z<>101

Too few commandline arguments.

=item Z<>102

Invalid command line options.

=item Z<>103

Error while setting the alarm signal handler.

=item Z<>104

Error while forking child process.

=item Z<>105

Error during exec(). Probably the command specified is not in the B<PATH>.

=item Z<>106

Either the I<command> timed out and was killed, or some other process sent
it a signal, causing it to terminate. The command did not return a useful
exit status.

=item Z<>107

Insufficient memory to perform requested task.

=item Z<>108

The I<time> specified was not a valid floating-point number.

=item Z<>109

The specified timer period was negative or zero.

=item Z<>110

The timer could not be set.

=item Z<>128 and up

The command was unsuccessful, and exited due to a signal (e.g. an interrupt
was sent from the keyboard).

=back

=head1 EXAMPLES

Attempt to download a network file, but allow at most 10 seconds:

    maxtime 10 wget http://catb.org/jargon/html/index.html

Request the status of a Baan shared memory segment, but do not lock up if
shared memory is corrupt:

    maxtime 2 $BSE/bin/shmmanager6.1 -s

Allow at most 10 minutes for stopping a Baan software environment:

    maxtime 600 $BSE/etc/rc.stop

=head1 BUGS

If the I<time> argument is omitted and an I<arg> argument is provided,
B<maxtime> might interpret its commandline arguments wrong. This will only
be apparent if B<-v> has been used, or if its exit status is tested for
value 105.

On some platforms, B<maxtime> might confuse options for the external
command as options for itself.  In these cases is necessary to indicate
the end of B<maxtime>'s options using C<-->.

=head1 SEE ALSO

exit(3), setitimer(2), signal(7), wait(2).

=head1 VERSION

This manual pertains to version 0.36.

=head1 AUTHOR and COPYRIGHT

=for roff
.\" the \(co macro only exists in groff
.ie \n(.g .ds co \(co
.el       .ds co (c)
.ie \n(.g .ds e' \('e
.el       .ds e' e\*'
.ie n Copyright (c) 2004-2019,
.el   Copyright \*(co 2004-2019,
Ren\*(e' Uittenbogaard (ruittenb@users.sourceforge.net).
.PP

This program is free software; you can redistribute it and/or modify it
under the terms described by the GNU General Public License version 3.

C<maxtime> is distributed without any warranty, even without the
implied warranties of merchantability or fitness for a particular purpose.

=cut

*/

// vim: set ai:

