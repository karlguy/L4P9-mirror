//%%%  systab.h %%%

//%----------------------------------------
#include "../../libc/9syscall-l4/sys.h"   //%

#include  <l4/types.h>       //070211

typedef long Syscall(ulong*, Clerkjob_t *);
typedef vlong Syscall_l4(ulong*, vlong*, Clerkjob_t *);  //% 061211, 070211

//%----------------------------------------

Syscall sysr1;
Syscall sys_errstr;
Syscall sysbind;
Syscall syschdir;
Syscall sysclose;
Syscall sysdup;
Syscall sysalarm;
Syscall sysexec;
Syscall sysexits;
//% Syscall sys_fsession;
//% Syscall sysfauth;
Syscall sys_fstat;
Syscall syssegbrk;
Syscall sys_mount;
Syscall sysopen;
Syscall sys_read;
Syscall sysoseek;
Syscall syssleep;
Syscall sys_stat;
Syscall sysrfork;
Syscall sys_write;
Syscall syspipe;
Syscall syscreate;
Syscall sysfd2path;
Syscall sysbrk_;
Syscall sysremove;
Syscall sys_wstat;
Syscall sys_fwstat;
Syscall sysnotify;
Syscall sysnoted;
Syscall syssegattach;
Syscall syssegdetach;
Syscall syssegfree;
//% Syscall syssegflush;
Syscall sysrendezvous;
Syscall sysunmount;
Syscall sys_wait;

Syscall_l4 sysseek_l4;   //% 061211

//% Syscall sysfversion;
Syscall syserrstr;
Syscall sysstat;
Syscall sysfstat;
Syscall syswstat;
Syscall sysfwstat;
Syscall sysmount;
Syscall sysawait;
Syscall syspread;
Syscall syspwrite;
Syscall	sysdeath;

Syscall	sys_d;    //%
Syscall	sys_put;    //%
Syscall	sys_get;    //%

Syscall	sysspawn;    //%

Syscall	sysgive;    //%
Syscall	systake;    //%

Syscall *systab[]={
	[SYSR1]		sysr1,
	[_ERRSTR]	sys_errstr,
	[BIND]		sysbind,
	[CHDIR]		syschdir,
	[CLOSE]		sysclose,
	[DUP]		sysdup,
	[ALARM]		sysalarm,
	[EXEC]		sysexec,
	[EXITS]		sysexits,
	//%	[_FSESSION]	sys_fsession,
	//%	[FAUTH]		sysfauth,
	[_FSTAT]	sys_fstat,
	[SEGBRK]	syssegbrk,
	[_MOUNT]	sys_mount,
	[OPEN]		sysopen,
	[_READ]		sys_read,
	[OSEEK]		sysoseek,
	[SLEEP]		syssleep,
	[_STAT]		sys_stat,
	[RFORK]		sysrfork,
	[_WRITE]	sys_write,
	[PIPE]		syspipe,
	[CREATE]	syscreate,
	[FD2PATH]	sysfd2path,
	[BRK_]		sysbrk_,
	[REMOVE]	sysremove,
	[_WSTAT]	sys_wstat,
	[_FWSTAT]	sys_fwstat,
	[NOTIFY]	sysnotify,
	[NOTED]		sysnoted,
	[SEGATTACH]	syssegattach,
	[SEGDETACH]	syssegdetach,
	[SEGFREE]	syssegfree,
	//%	[SEGFLUSH]	syssegflush,
	[RENDEZVOUS]	sysrendezvous,
	[UNMOUNT]	sysunmount,
	[_WAIT]		sys_wait,
	[SEEK]		sysseek_l4,  //% 061211 Type warning
	//%	[FVERSION]	sysfversion,
	[ERRSTR]	syserrstr,
	[STAT]		sysstat,
	[FSTAT]		sysfstat,
	[WSTAT]		syswstat,
	[FWSTAT]	sysfwstat,
	[MOUNT]		sysmount,
	[AWAIT]		sysawait,
	[PREAD]		syspread,
	[PWRITE]	syspwrite,

	[_D]            sys_d,      //%
	[_PUT]          sys_put,      //%
	[_GET]          sys_get,      //%

	[SPAWN]          sysspawn,      //%

	[GIVE]          sysgive,      //%
	[TAKE]          systake,      //%
};

char *sysctab[]={
	[SYSR1]		"Running",
	[_ERRSTR]	"_errstr",
	[BIND]		"Bind",
	[CHDIR]		"Chdir",
	[CLOSE]		"Close",
	[DUP]		"Dup",
	[ALARM]		"Alarm",
	[EXEC]		"Exec",
	[EXITS]		"Exits",
	[_FSESSION]	"_fsession",
	[FAUTH]		"Fauth",
	[_FSTAT]	"_fstat",
	[SEGBRK]	"Segbrk",
	[_MOUNT]	"_mount",
	[OPEN]		"Open",
	[_READ]		"_read",
	[OSEEK]		"Oseek",
	[SLEEP]		"Sleep",
	[_STAT]		"_stat",
	[RFORK]		"Rfork",
	[_WRITE]	"_write",
	[PIPE]		"Pipe",
	[CREATE]	"Create",
	[FD2PATH]	"Fd2path",
	[BRK_]		"Brk",
	[REMOVE]	"Remove",
	[_WSTAT]	"_wstat",
	[_FWSTAT]	"_fwstat",
	[NOTIFY]	"Notify",
	[NOTED]		"Noted",
	[SEGATTACH]	"Segattach",
	[SEGDETACH]	"Segdetach",
	[SEGFREE]	"Segfree",
	[SEGFLUSH]	"Segflush",
	[RENDEZVOUS]	"Rendez",
	[UNMOUNT]	"Unmount",
	[_WAIT]		"_wait",
	[SEEK]		"Seek",
	[FVERSION]	"Fversion",
	[ERRSTR]	"Errstr",
	[STAT]		"Stat",
	[FSTAT]		"Fstat",
	[WSTAT]		"Wstat",
	[FWSTAT]	"Fwstat",
	[MOUNT]		"Mount",
	[AWAIT]		"Await",
	[PREAD]		"Pread",
	[PWRITE]	"Pwrite",

	[_D]            "_d",      //%
	[_PUT]          "_put",      //%
	[_GET]          "_get",      //%

	[SPAWN]         "Spawn",     //%

	[GIVE]          "give",      //%
	[TAKE]          "take",      //%
};

int nsyscall = (sizeof systab/sizeof systab[0]);
