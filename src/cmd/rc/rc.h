//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/*
 * Plan9 is defined for plan 9
 * V9 is defined for 9th edition
 * Sun is defined for sun-os
 * Please don't litter the code with ifdefs.  The three below (and one in
 * getflags) should be enough.
 */
#define	Plan9

#ifdef	Plan9
#include <u.h>
#include <libc.h>
#define	NSIG	32
#define	SIGINT	2
#define	SIGQUIT	3
#endif

#ifdef	V9
#include <signal.h>
#include <libc.h>
#endif

#ifdef Sun
#include <signal.h>
#endif

#define	YYMAXDEPTH	500
#ifndef PAREN
//%  #include "x.tab.h"  //%original
#include "y.tab.h"
#endif

typedef struct tree tree;
typedef struct word word;
//%  typedef struct io io;
typedef union code code;
typedef struct var var;
typedef struct list list;
typedef struct redir redir;
typedef struct thread thread;
typedef struct builtin builtin;

//%  #pragma incomplete word
//%  #pragma incomplete io

struct tree{
	int type;
	int rtype, fd0, fd1;		/* details of REDIR PIPE DUP tokens */
	char *str;
	int quoted;
	int iskw;
	tree *child[3];
	tree *next;
};
tree *newtree(void);
tree *token(char*, int), *klook(char*), *tree1(int, tree*);
tree *tree2(int, tree*, tree*), *tree3(int, tree*, tree*, tree*);
tree *mung1(tree*, tree*), *mung2(tree*, tree*, tree*);
tree *mung3(tree*, tree*, tree*, tree*), *epimung(tree*, tree*);
tree *simplemung(tree*), *heredoc(tree*);
void freetree(tree*);
tree *cmdtree;
/*
 * The first word of any code vector is a reference count.
 * Always create a new reference to a code vector by calling codecopy(.).
 * Always call codefree(.) when deleting a reference.
 */
union code{
	void (*f)(void);
	int i;
	char *s;
};
char *promptstr;
int doprompt;
#define	NTOK	8192
char tok[NTOK];
#define	APPEND	1
#define	WRITE	2
#define	READ	3
#define	HERE	4
#define	DUPFD	5
#define	CLOSE	6
#define RDWR	7
struct var{
	char *name;		/* ascii name */
	word *val;	/* value */
	int changed;
	code *fn;		/* pointer to function's code vector */
	int fnchanged;
	int pc;			/* pc of start of function */
	var *next;	/* next on hash or local list */
};
var *vlook(char*), *gvlook(char*), *newvar(char*, var*);
#define	NVAR	521
var *gvar[NVAR];				/* hash for globals */
#define	new(type)	((type *)emalloc(sizeof(type)))
char *emalloc(long);
void *Malloc(ulong);
void efree(char*);
#define	NOFILE	128		/* should come from <param.h> */
struct here{
	tree *tag;
	char *name;
	struct here *next;
};
int mypid;
/*
 * Glob character escape in strings:
 *	In a string, GLOB must be followed by *?[ or GLOB.
 *	GLOB* matches any string
 *	GLOB? matches any single character
 *	GLOB[...] matches anything in the brackets
 *	GLOBGLOB matches GLOB
 */
#define	GLOB	((char)0x01)
/*
 * onebyte(c), twobyte(c), threebyte(c)
 * Is c the first character of a one- two- or three-byte utf sequence?
 */
#define	onebyte(c)	((c&0x80)==0x00)
#define	twobyte(c)	((c&0xe0)==0xc0)
#define	threebyte(c)	((c&0xf0)==0xe0)
char **argp;
char **args;
int nerror;		/* number of errors encountered during compilation */
int doprompt;		/* is it time for a prompt? */
/*
 * Which fds are the reading/writing end of a pipe?
 * Unfortunately, this can vary from system to system.
 * 9th edition Unix doesn't care, the following defines
 * work on plan 9.
 */
#define	PRD	0
#define	PWR	1

//% char Rcmain[], Fdprefix[];      //% original
extern char Rcmain[], Fdprefix[];   //% extern

#define	register
/*
 * How many dot commands have we executed?
 * Used to ensure that -v flag doesn't print rcmain.
 */
int ndot;
char *getstatus(void);
int lastc;
int lastword;

#if 1 //% DBUGGING --------------------------------------
/*  
 * A user of DBGPRN/ZZ must define _DBGFLG in advance:
 *  #define   _DBGFLG 1      //or:  static int _DBGFLG = 1;
 */
 
 
#define  DBGPRN  if(_DBGFLG) print
#define  FF      if(_DBGFLG) print(">%s()\n", __FUNCTION__);
#define  FF2     if(_DBGFLG) print("<%s()\n", __FUNCTION__);
#define  DBGARG(a) if(_DBGFLG){struct word *wp; wp=a; \
 print("--- "); while(wp){print("%s ", wp->word);wp=wp->next;} print("---\n"); }

#define L4_KDB_Enter(str...)                    \
  do {                                          \
    __asm__ __volatile__ (                      \
	"/* L4_KDB_Enter() */           \n"     \
        "       int     $3              \n"     \
        "       jmp     2f              \n"     \
        "       mov     $1f, %eax       \n"     \
        ".section .rodata               \n"     \
        "1:     .ascii \"" str "\"      \n"     \
        "       .byte 0                 \n"     \
        ".previous                      \n"     \
	"2:                             \n");   \
  } while (0)


#endif //%--------------------------------------
