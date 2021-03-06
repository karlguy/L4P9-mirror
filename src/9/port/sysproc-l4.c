//%%%%%%% sysproc-l4.c %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include	"u.h"
#include	"tos.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"
#include	"edf.h"
//% #include	<a.out.h>  //% ELF files are used.

#if 1 //%-----------------------------------------------------
#include       <l4all.h>
#include       <lp49/lp49.h>
#include        "../errhandler-l4.h"

#define        _DBGFLG  0
#include       <l4/_dbgpr.h>
#define        PRN    l4printf_b

extern int core_process_nr;
extern Proc* tid2pcb(L4_ThreadId_t);
extern int start_prog(Proc *pcb, L4_ThreadId_t, char *, L4_ThreadId_t, 
		      unsigned, int, int);
extern int proc2proc_copy(unsigned fromproc, unsigned fromadrs,
			  unsigned toproc, unsigned toadrs, int  size);
extern void  l4printf(char*, ...);
extern int _stkmargin(Clerkjob_t *myjob);
#endif //----------------------------------------------------

int	shargs(char*, int, char**);

extern void checkpages(void);
extern void checkpagerefs(void);


long sysr1(ulong* _x, Clerkjob_t *myjob)
{
        TBD;
#if 0 
	checkpagerefs();
#endif
	return 0;
}


long 
sysrfork(ulong *arg, Clerkjob_t *myjob)  //% ONERR,  
// cf. libc/*/rfork-l4.c  
// arg[0]: flag, arg[1]: IP, arg[2]:SP
{
	Proc *p;
	//	int n, i;
	Fgrp *ofg;
	Pgrp *opg;
	Rgrp *org;
	Egrp *oeg;
	ulong pid, flag;
	//%	Mach *wm;
DBGPRN(">%s(flag;%x ip:%x sp:%x)\n", __FUNCTION__, arg[0], arg[1], arg[2]);

	flag = arg[0];

	/* Check flags before we commit */
	if((flag & (RFFDG|RFCFDG)) == (RFFDG|RFCFDG))
	        ERROR_RETURN(Ebadarg, ONERR);  //% error(Ebadarg);
	if((flag & (RFNAMEG|RFCNAMEG)) == (RFNAMEG|RFCNAMEG))
		ERROR_RETURN(Ebadarg, ONERR);  //% error(Ebadarg);
	if((flag & (RFENVG|RFCENVG)) == (RFENVG|RFCENVG))
		ERROR_RETURN(Ebadarg, ONERR);  //% error(Ebadarg);

	if((flag&RFPROC) == 0) {
		if(flag & (RFMEM|RFNOWAIT))
			ERROR_RETURN(Ebadarg, ONERR);  //% error(Ebadarg);
		if(flag & (RFFDG|RFCFDG)) {
			ofg = up->fgrp;
			if(flag & RFFDG)
				up->fgrp = dupfgrp(ofg);
			else
				up->fgrp = dupfgrp(nil);
			closefgrp(ofg);
		}
		if(flag & (RFNAMEG|RFCNAMEG)) {
			opg = up->pgrp;
			up->pgrp = newpgrp();
			if(flag & RFNAMEG)
				pgrpcpy(up->pgrp, opg);
			/* inherit noattach */
			up->pgrp->noattach = opg->noattach;
			closepgrp(opg);
		}
		if(flag & RFNOMNT)
			up->pgrp->noattach = 1;
		if(flag & RFREND) {
			org = up->rgrp;
			up->rgrp = newrgrp();
			closergrp(org);
		}
		if(flag & (RFENVG|RFCENVG)) {
			oeg = up->egrp;
			up->egrp = smalloc(sizeof(Egrp));
			up->egrp->_ref.ref = 1;   //%  up->egrp->ref = 1;
			if(flag & RFENVG)
				envcpy(up->egrp, oeg);
			closeegrp(oeg);
		}
		if(flag & RFNOTEG)
			up->noteid = incref(&noteidalloc);
		return 0;
	}

	p = newproc();

	p->fpsave = up->fpsave;
	p->scallnr = up->scallnr;
	p->s = up->s;
	p->nerrlab = 0;
	p->slash = up->slash;
	p->dot = up->dot;
	incref(& p->dot->_ref);	  //% incref(p->dot);

	//% memmove(p->note, up->note, sizeof(p->note));  //% ?
	p->privatemem = up->privatemem;
	p->noswap = up->noswap; 
	p->nnote = up->nnote;
	p->notified = 0;
	p->lastnote = up->lastnote;
	p->notify = up->notify;
	p->ureg = up->ureg;  //% ?
	p->dbgreg = 0;       //% ?

#if 0 //%  -----------------------
	/* Make a new set of memory segments */
	n = flag & RFMEM;
	qlock(&p->seglock);
	if(WASERROR()){   //%
	_ERR_1:
		qunlock(&p->seglock);
		NEXTERROR_RETURN(ONERR);   //% nexterror();
	}
	for(i = 0; i < NSEG; i++)
		if(up->seg[i])
			p->seg[i] = dupseg(up->seg, i, n);
	qunlock(&p->seglock);
	PORERROR();  //% poperror();
#endif //% -------------------------

	/* File descriptors */
	if(flag & (RFFDG|RFCFDG)) {
		if(flag & RFFDG)
			p->fgrp = dupfgrp(up->fgrp);
		else
			p->fgrp = dupfgrp(nil);
	}
	else {
		p->fgrp = up->fgrp;
		incref(& p->fgrp->_ref);	//%  incref(p->fgrp);
	}


	/* Process groups */
	if(flag & (RFNAMEG|RFCNAMEG)) {
		p->pgrp = newpgrp();
		if(flag & RFNAMEG)
			pgrpcpy(p->pgrp, up->pgrp);
		/* inherit noattach */
		p->pgrp->noattach = up->pgrp->noattach;
	}
	else {
		p->pgrp = up->pgrp;
		incref(& p->pgrp->_ref);	//% incref(p->pgrp);
	}
	if(flag & RFNOMNT)
		up->pgrp->noattach = 1;

	if(flag & RFREND)
		p->rgrp = newrgrp();
	else {
	        incref(& up->rgrp->_ref);  //% incref(up->rgrp);
		p->rgrp = up->rgrp;
	}


	/* Environment group */
	if(flag & (RFENVG|RFCENVG)) {
		p->egrp = smalloc(sizeof(Egrp));
		p->egrp->_ref.ref = 1;	//% p->egrp->ref = 1;
		if(flag & RFENVG)
			envcpy(p->egrp, up->egrp);
	}
	else {
		p->egrp = up->egrp;
		incref(& p->egrp->_ref);	//% incref(p->egrp);
	}
	p->hang = up->hang;
	p->procmode = up->procmode;

#if 0 //% -------------------------
	/* Craft a return frame which will cause the child to pop out of
	 * the scheduler in user mode with the return register zero
	 */
	forkchild(p, up->dbgreg);
#endif //% ---------------------------

	p->parent = up;
	p->parentpid = up->pid;

	if(flag&RFNOWAIT)
		p->parentpid = 0;
	else {
		lock(&up->exl);
		up->nchild++;
		unlock(&up->exl);
	}

	if((flag&RFNOTEG) == 0)
		p->noteid = up->noteid;

	p->fpstate = up->fpstate;
	pid = p->pid;
	memset(p->time, 0, sizeof(p->time));
	//%	p->time[TReal] = MACHP(0)->ticks;

	kstrdup(&p->text, up->text);
	kstrdup(&p->user, up->user);

#if 0 //% ---------------------------
	/*
	 *  since the bss/data segments are now shareable,
	 *  any mmu info about this process is now stale
	 *  (i.e. has bad properties) and has to be discarded.
	 */
	flushmmu();
	p->basepri = up->basepri;
	p->priority = up->basepri;
	p->fixedpri = up->fixedpri;
	p->mp = up->mp;
	wm = up->wired;
	if(wm)
		procwired(p, wm->machno);
	ready(p);
	sched();
#endif //% --------------------------

#if 1 //% ---------------------------------
	{
	  L4_ThreadId_t parent_tid = up->thread;
	  L4_ThreadId_t child_tid  = p->thread;
	  int         th_max       = 32;     //16  Mux # of threads ?
	  L4_ThreadId_t  pager = L4_Pager(); 
	  void  *ip = (void*)arg[1];   //!!
	  void  *sp = (void*)arg[2];   //!!

	  L4_MsgTag_t tag;
	  L4_Msg_t    _MRs;
 
	  DBGPRN("mx_pager <-mm_fork(%x %x %d %x %x %x)\n", 
		 parent_tid.raw, child_tid.raw, th_max, pager.raw, ip, sp);

	  L4_MsgClear (&_MRs);
	  L4_MsgAppendWord (&_MRs, parent_tid.raw);   //MR1
	  L4_MsgAppendWord (&_MRs, child_tid.raw);    //MR2
	  L4_MsgAppendWord (&_MRs, th_max);   //MR3
	  L4_MsgAppendWord (&_MRs, pager.raw);   //MR4
	  L4_MsgAppendWord (&_MRs, (L4_Word_t)ip);   //MR5
	  L4_MsgAppendWord (&_MRs, (L4_Word_t)sp);   //MR6

	  L4_Set_MsgLabel (&_MRs, MM_FORK); //MR0: label
	  L4_MsgLoad (&_MRs);

	  tag = L4_Call (pager);    

	  if (L4_IpcFailed(tag))
	      L4_KDB_Enter("MM_FORK");
 
	  L4_ThreadSwitch(child_tid);  //090717

	  L4_MsgStore(tag, &_MRs);
	  DBGPRN("<%s()=> pid=%d tag=%x \n", __FUNCTION__, pid, tag);
	}
#endif //%---------------------------------

	return pid;
}


#if 0 
static ulong l2be(long l)
{
	uchar *cp;
	cp = (uchar*)&l;
	return (cp[0]<<24) | (cp[1]<<16) | (cp[2]<<8) | cp[3];
}
#endif

/* To be extended:
 *  (1) Elf file check
 *  (2) "#!" script file
 *  (3) Stack segment
 *  (4) Close on exec.
 */

//    arg[0]: pathname, arg[1]: argtbl_adrs, arg[2]: argtbl_size. 
long 
sysexec(ulong *arg, Clerkjob_t *myjob) //%  ELF file.
{
	Chan *chan;
	Proc *pcb;
	unsigned  argtbl_adrs = arg[1];
	int  argtbl_size = arg[2];
	L4_ThreadId_t  client = myjob->client;  //%
	int  rc;

DBGPRN(">%s('%s' %x %d %x)\n", __FUNCTION__, arg[0], arg[1], arg[2], client);
	// _stkmargin(myjob); //%%  Does stack has enough area ?

	if (client.raw != up->thread.raw) 
	    l4printf_b("! sysproc() threadid unmatch\n");

	pcb = tid2pcb(client);
	rc = start_prog(pcb, client, (char*)arg[0], client, 
			(unsigned)argtbl_adrs, argtbl_size, 0); 
	if (rc == ONERR)  
	    return  ONERR;

	//	kstrdup(&pcb->text, (char*)arg[0]); 
	DBGPRN("<sysexec() \n");
	return 0;
}


//  arg[0]:pathname,  arg[1]:argtbl-address, arg[2]:argtbl-size
long 
sysspawn(ulong *arg, Clerkjob_t *myjob) //% ONERR,  adopted to the ELF file.
{
	Chan *chan;
        Proc *pcb;
        ulong pid, flag;
	unsigned  argtbl_adrs = arg[1];
	int  argtbl_size = arg[2];
	L4_ThreadId_t  client = myjob->client;  //%
	int  rc;
DBGPRN(">%s('%s' %x %d)\n", __FUNCTION__, arg[0], arg[1], arg[2]);

	flag = RFFDG | RFREND | RFPROC;  
        pcb = newproc(); 	
                                                                               
        pcb->fpsave = up->fpsave;
        pcb->scallnr = up->scallnr;
        pcb->s = up->s;
        pcb->nerrlab = 0;
        pcb->slash = up->slash;
        pcb->dot = up->dot;
        incref(& pcb->dot->_ref);   //% incref(p->dot);
                                                                               
	//-----------------------------------
        /* File descriptors */
        if(flag & (RFFDG|RFCFDG)) {
	    if(flag & RFFDG)
	        pcb->fgrp = dupfgrp(up->fgrp);
	    else
	        pcb->fgrp = dupfgrp(nil);
        }
        else {
	      pcb->fgrp = up->fgrp;
	      incref(& pcb->fgrp->_ref);        //%  incref(p->fgrp);
        }
                                                                               
        /* Process groups */
        if(flag & (RFNAMEG|RFCNAMEG)) {
	    pcb->pgrp = newpgrp();
	    if(flag & RFNAMEG)
	        pgrpcpy(pcb->pgrp, up->pgrp);
	    /* inherit noattach */
	    pcb->pgrp->noattach = up->pgrp->noattach;
        }
        else {
	    pcb->pgrp = up->pgrp;
	    incref(& pcb->pgrp->_ref);        //% incref(p->pgrp);
        }
        if(flag & RFNOMNT)
	    up->pgrp->noattach = 1;

        if(flag & RFREND)
	    pcb->rgrp = newrgrp();
        else {
	    incref(& up->rgrp->_ref);  //% incref(up->rgrp);
	    pcb->rgrp = up->rgrp;
        }

        /* Environment group */
        if(flag & (RFENVG|RFCENVG)) {
	    pcb->egrp = smalloc(sizeof(Egrp));
	    pcb->egrp->_ref.ref = 1;  //% pcb->egrp->ref = 1;
	    if(flag & RFENVG)
	        envcpy(pcb->egrp, up->egrp);
        }
        else {
	    pcb->egrp = up->egrp;
	    incref(& pcb->egrp->_ref);        //% incref(pcb->egrp);
        }

        pcb->hang = up->hang;
        pcb->procmode = up->procmode;
                                                                                
	//---------------------
        pcb->parent = up;
        pcb->parentpid = up->pid;
        if(flag&RFNOWAIT)
	    pcb->parentpid = 0;
        else {
	    lock(&up->exl);
	    up->nchild++;
	    unlock(&up->exl);
        }

        if((flag&RFNOTEG) == 0)
	    pcb->noteid = up->noteid;

        pcb->fpstate = up->fpstate;
        pid = pcb->pid;
        memset(pcb->time, 0, sizeof(pcb->time));
        //%     pcb->time[TReal] = MACHP(0)->ticks;

	//  kstrdup(&pcb->text, up->text);
        kstrdup(&pcb->user, up->user);

	//----------------
	if (client.raw != up->thread.raw) 
	    l4printf_b("! sysproc() threadid unmatch\n");

	up = pcb;

	DBGPRN("sysspawn tid:%x\n", up->thread.raw);
	rc = start_prog(pcb, pcb->thread, (char*)arg[0], client, 
			argtbl_adrs, argtbl_size, 1); 
	if (rc == ONERR){
	    return  ONERR;
	}
	//kstrdup(&pcb->text, (char*)arg[0]); 

	DBGPRN("< sysspawn() \n");
	return pid;
}


int shargs(char *s, int n, char **ap)
{
	int i;

	s += 2;
	n -= 2;		/* skip #! */
	for(i=0; s[i]!='\n'; i++)
		if(i == n-1)
			return 0;
	s[i] = 0;
	*ap = 0;
	i = 0;
	for(;;) {
		while(*s==' ' || *s=='\t')
			s++;
		if(*s == 0)
			break;
		i++;
		*ap++ = s;
		*ap = 0;
		while(*s && *s!=' ' && *s!='\t')
			s++;
		if(*s == 0)
			break;
		else
			*s++ = 0;
	}
	return i;
}


int return0(void* _x)  //%
{
	return 0;
}


long syssleep(ulong *arg, Clerkjob_t *myjob) //%
{
        DBGPRN(">%s \n", __FUNCTION__);
	int n;

	n = arg[0];
	if(n <= 0) {
	        //% if (up->edf && (up->edf->flags & Admitted))
	        //%	edfyield();
	        //% else
	        L4_Yield();  //% yield();
		return 0;
	}
	if(n < TK2MS(1))
		n = TK2MS(1);
	tsleep(&up->sleep, return0, 0, n);
	return 0;
}


long sysalarm(ulong *arg, Clerkjob_t *myjob) //%
{        
        TBD;   return 0;
#if 0	
	return procalarm(arg[0]);
#endif 
}

// exits(char* msg)

long  sysexits(ulong *arg, Clerkjob_t *myjob) //%
{
	char *status;
	char *inval = "invalid exit string";
	char buf[ERRMAX];

	status = (char*)arg[0];
	if(status && status[1]){    //% original:  if(status)
	        if(WASERROR()) { //%
		_ERR_1:
			status = inval;
		}
		else{
		        //%	validaddr((ulong)status, 1, 0);
			if(vmemchr(status, 0, ERRMAX) == 0){
				memmove(buf, status, ERRMAX);
				buf[ERRMAX-1] = 0;
				status = buf;
			}
			POPERROR();  //% poperror();
		}

	}
	pexit(status, 1);
	return 0;		/* not reached */
}


long sys_wait(ulong *arg, Clerkjob_t *myjob) //%
{
	int pid;
	Waitmsg w;
	OWaitmsg *ow;

	if(arg[0] == 0)
		return pwait(nil);

	validaddr(arg[0], sizeof(OWaitmsg), 1);
	evenaddr(arg[0]);
	pid = pwait(&w);
	if(pid >= 0){
		ow = (OWaitmsg*)arg[0];
		readnum(0, ow->pid, NUMSIZE, w.pid, NUMSIZE);
		readnum(0, ow->time+TUser*NUMSIZE, NUMSIZE, w.time[TUser], NUMSIZE);
		readnum(0, ow->time+TSys*NUMSIZE, NUMSIZE, w.time[TSys], NUMSIZE);
		readnum(0, ow->time+TReal*NUMSIZE, NUMSIZE, w.time[TReal], NUMSIZE);
		strncpy(ow->msg, w.msg, sizeof(ow->msg));
		ow->msg[sizeof(ow->msg)-1] = '\0';
	}
	return pid;
}


long sysawait(ulong *arg, Clerkjob_t *myjob) //%
{
	int i;
	int pid;
	Waitmsg w;
	ulong n;
#if 1 //%----------------------
	uint adrs = (arg[0]-L4_Address(myjob->mapper)) + L4_Address(myjob->mappee); //%
#else 
	uint adrs = arg[0];
#endif  //%-------------------

	n = arg[1];
	//%  validaddr(arg[0], n, 1);

DBGPRN(">sysawait(%x %x)\n", adrs, n);

	pid = pwait(&w);
	if(pid < 0)
		return -1;
	i = snprint((char*)adrs /*arg[0]*/, n, "%d %lud %lud %lud %q",
		w.pid,
		w.time[TUser], w.time[TSys], w.time[TReal],
		w.msg);

	return i;
}


long sysdeath(ulong*  _x)  //%
{
        //%	pprint("deprecated system call\n");
	pexit("Suicide", 0);
	return 0;	/* not reached */
}


void werrstr(char *fmt, ...)
{
	va_list va;

	if(up == nil)
		return;

	va_start(va, fmt);
	vseprint(up->syserrstr, up->syserrstr+ERRMAX, fmt, va);
	va_end(va);
}


static long generrstr(char *buf, uint nbuf, Clerkjob_t *myjob) //% ONERR
{
	char tmp[ERRMAX];
	L4_ThreadId_t  client = myjob->client;  //%

	DBGPRN(">generrstr(%x:%s, %d)\n", buf, buf, nbuf);
	if(nbuf == 0)
		ERROR_RETURN(Ebadarg, ONERR);  //% error(Ebadarg);
	//%	validaddr((ulong)buf, nbuf, 1);
	if(nbuf > sizeof tmp)
		nbuf = sizeof tmp;
#if 1 //%----------------------------------------
	proc2proc_copy(TID2PROCNR(client), (uint)buf, core_process_nr, (uint)tmp, nbuf);
	tmp[nbuf-1] = '\0';

	up->errstr[nbuf-1] = '\0';  //? syserrstr
	proc2proc_copy(core_process_nr, (uint)up->errstr, TID2PROCNR(client), (uint)buf, nbuf);
	// l4printf_b("!genstrerr %s\n", up->errstr);  //? syserrstr
	
	memmove(up->errstr, tmp, nbuf);  //? syserrstr
#else //%----------------------------------------
	memmove(tmp, buf, nbuf);

	/* make sure it's NUL-terminated */
	tmp[nbuf-1] = '\0';
	memmove(buf, up->syserrstr, nbuf);
	buf[nbuf-1] = '\0';
	memmove(up->syserrstr, tmp, nbuf);
#endif //%----------------------------------------
	return 0;
}


long syserrstr(ulong *arg, Clerkjob_t *myjob) //% ONERR
{
        return generrstr((char*)arg[0], arg[1], myjob); //%
}

/* compatibility for old binaries */

long sys_errstr(ulong *arg, Clerkjob_t *myjob) //% ONERR
{
        return generrstr((char*)arg[0], 64, myjob); //%
}


long sysnotify(ulong *arg, Clerkjob_t *myjob) //% 
{
	if(arg[0] != 0)
		validaddr(arg[0], sizeof(ulong), 0);
	up->notify = (int(*)(void*, char*))(arg[0]);
	return 0;
}


long sysnoted(ulong *arg, Clerkjob_t *myjob) //%
{
	if(arg[0]!=NRSTR && !up->notified)
	        ERROR_RETURN(Egreg, ONERR);  //% error(Egreg);
	return 0;
}


long syssegbrk(ulong *arg, Clerkjob_t *myjob) //%
{
        return 0;
}


long syssegattach(ulong *arg, Clerkjob_t *myjob) //%
{
        return 0;
}


long syssegdetach(ulong *arg, Clerkjob_t *myjob) //%
{
        return 0;
}


long syssegfree(ulong *arg, Clerkjob_t *myjob) //%
{
        return 0;
}


/* For binary compatibility */

long sysbrk_(ulong *arg, Clerkjob_t *myjob) //%
{
        TBD;	return 0;
#if 0
	return ibrk(arg[0], BSEG);
#endif
}


long sysrendezvous(ulong *arg, Clerkjob_t *myjob) //%
{
	uintptr tag, val;
	Proc *p, **l;

	tag = arg[0];
	l = &REND(up->rgrp, tag);
	up->rendval = ~(uintptr)0;

	lock(& up->rgrp->_ref._lock);	  //% lock(up->rgrp);  
	for(p = *l; p; p = p->rendhash) {
		if(p->rendtag == tag) {
			*l = p->rendhash;
			val = p->rendval;
			p->rendval = arg[1];
#if 1 //% ------------------------------
			L4_ThreadSwitch(p->thread); 
#else //original--------------------------
			while(p->mach != 0) ;
			ready(p);
#endif //%--------------------------------
			unlock(& up->rgrp->_ref._lock);  //% unlock(up->rgrp);
			return val;
		}
		l = &p->rendhash;
	}

	/* Going to sleep here */
	up->rendtag = tag;
	up->rendval = arg[1];
	up->rendhash = *l;
	*l = up;
	up->state = Rendezvous;
	unlock(& up->rgrp->_ref._lock);    //% unlock(up->rgrp);  
#if 1 //% -------------------
	L4_Yield();
#else //original-------------
	sched();
#endif //%-------------------
	return up->rendval;
}



#if 0 // plan9 for reference -----------------------------
long 
sysexec(ulong *arg) 
{
	Segment *s, *ts;
	ulong t, d, b;
	int i;
	Chan *tc;
	char **argv, **argp;
	char *a, *charp, *args, *file;
	char *progarg[sizeof(Exec)/2+1], *elem, progelem[64];
	ulong ssize, spage, nargs, nbytes, n, bssend;
	int indir;
	Exec exec;
	char line[sizeof(Exec)];
	Fgrp *f;
	Image *img;
	ulong magic, text, entry, data, bss;
	Tos *tos;

	validaddr(arg[0], 1, 0);
	file = (char*)arg[0];
	indir = 0;
	elem = nil;
	if(waserror()){
		free(elem);
		nexterror();
	}
	for(;;){
		tc = namec(file, Aopen, OEXEC, 0);
		if(waserror()){
			cclose(tc);
			nexterror();
		}
		if(!indir)
			kstrdup(&elem, up->genbuf);

		n = devtab[tc->type]->read(tc, &exec, sizeof(Exec), 0);
		if(n < 2)
			error(Ebadexec);
		magic = l2be(exec.magic);
		text = l2be(exec.text);
		entry = l2be(exec.entry);
		if(n==sizeof(Exec) && (magic == AOUT_MAGIC)){
			if(text >= USTKTOP-UTZERO
			|| entry < UTZERO+sizeof(Exec)
			|| entry >= UTZERO+sizeof(Exec)+text)
				error(Ebadexec);
			break; /* for binary */
		}

		/*
		 * Process #! /bin/sh args ...
		 */
		memmove(line, &exec, sizeof(Exec));
		if(indir || line[0]!='#' || line[1]!='!')
			error(Ebadexec);
		n = shargs(line, n, progarg);
		if(n == 0)
			error(Ebadexec);
		indir = 1;
		/*
		 * First arg becomes complete file name
		 */
		progarg[n++] = file;
		progarg[n] = 0;
		validaddr(arg[1], BY2WD, 1);
		arg[1] += BY2WD;
		file = progarg[0];
		if(strlen(elem) >= sizeof progelem)
			error(Ebadexec);
		strcpy(progelem, elem);
		progarg[0] = progelem;
		poperror();
		cclose(tc);
	}

	data = l2be(exec.data);
	bss = l2be(exec.bss);
	t = (UTZERO+sizeof(Exec)+text+(BY2PG-1)) & ~(BY2PG-1);
	d = (t + data + (BY2PG-1)) & ~(BY2PG-1);
	bssend = t + data + bss;
	b = (bssend + (BY2PG-1)) & ~(BY2PG-1);
	if(t >= KZERO || d >= KZERO || b >= KZERO)
		error(Ebadexec);

	/*
	 * Args: pass 1: count
	 */
	nbytes = sizeof(Tos);	/* hole for profiling clock at top of stack (and more) */
	nargs = 0;
	if(indir){
		argp = progarg;
		while(*argp){
			a = *argp++;
			nbytes += strlen(a) + 1;
			nargs++;
		}
	}
	evenaddr(arg[1]);
	argp = (char**)arg[1];
	validaddr((ulong)argp, BY2WD, 0);
	while(*argp){
		a = *argp++;
		if(((ulong)argp&(BY2PG-1)) < BY2WD)
			validaddr((ulong)argp, BY2WD, 0);
		validaddr((ulong)a, 1, 0);
		nbytes += ((char*)vmemchr(a, 0, 0x7FFFFFFF) - a) + 1;
		nargs++;
	}
	ssize = BY2WD*(nargs+1) + ((nbytes+(BY2WD-1)) & ~(BY2WD-1));

	/*
	 * 8-byte align SP for those (e.g. sparc) that need it.
	 * execregs() will subtract another 4 bytes for argc.
	 */
	if((ssize+4) & 7)
		ssize += 4;
	spage = (ssize+(BY2PG-1)) >> PGSHIFT;

	/*
	 * Build the stack segment, putting it in kernel virtual for the moment
	 */
	if(spage > TSTKSIZ)
		error(Enovmem);

	qlock(&up->seglock);
	if(waserror()){
		qunlock(&up->seglock);
		nexterror();
	}
	up->seg[ESEG] = newseg(SG_STACK, TSTKTOP-USTKSIZE, USTKSIZE/BY2PG);

	/*
	 * Args: pass 2: assemble; the pages will be faulted in
	 */
	tos = (Tos*)(TSTKTOP - sizeof(Tos));
	tos->cyclefreq = m->cyclefreq;
	cycles((uvlong*)&tos->pcycles);
	tos->pcycles = -tos->pcycles;
	tos->kcycles = tos->pcycles;
	tos->clock = 0;
	argv = (char**)(TSTKTOP - ssize);
	charp = (char*)(TSTKTOP - nbytes);
	args = charp;
	if(indir)
		argp = progarg;
	else
		argp = (char**)arg[1];

	for(i=0; i<nargs; i++){
		if(indir && *argp==0) {
			indir = 0;
			argp = (char**)arg[1];
		}
		*argv++ = charp + (USTKTOP-TSTKTOP);
		n = strlen(*argp) + 1;
		memmove(charp, *argp++, n);
		charp += n;
	}

	free(up->text);
	up->text = elem;
	elem = nil;	/* so waserror() won't free elem */
	//%	USED(elem);

	/* copy args; easiest from new process's stack */
	n = charp - args;
	if(n > 128)	/* don't waste too much space on huge arg lists */
		n = 128;
	a = up->args;
	up->args = nil;
	free(a);
	up->args = smalloc(n);
	memmove(up->args, args, n);
	if(n>0 && up->args[n-1]!='\0'){
		/* make sure last arg is NUL-terminated */
		/* put NUL at UTF-8 character boundary */
		for(i=n-1; i>0; --i)
			if(fullrune(up->args+i, n-i))
				break;
		up->args[i] = 0;
		n = i+1;
	}
	up->nargs = n;

	/*
	 * Committed.
	 * Free old memory.
	 * Special segments are maintained across exec
	 */
	for(i = SSEG; i <= BSEG; i++) {
		putseg(up->seg[i]);
		/* prevent a second free if we have an error */
		up->seg[i] = 0;
	}
	for(i = BSEG+1; i < NSEG; i++) {
		s = up->seg[i];
		if(s != 0 && (s->type&SG_CEXEC)) {
			putseg(s);
			up->seg[i] = 0;
		}
	}

	/*
	 * Close on exec
	 */
	f = up->fgrp;
	for(i=0; i<=f->maxfd; i++)
		fdclose(i, CCEXEC);

	/* Text.  Shared. Attaches to cache image if possible */
	/* attachimage returns a locked cache image */
	img = attachimage(SG_TEXT|SG_RONLY, tc, UTZERO, (t-UTZERO)>>PGSHIFT);
	ts = img->s;
	up->seg[TSEG] = ts;
	ts->flushme = 1;
	ts->fstart = 0;
	ts->flen = sizeof(Exec)+text;
	unlock(& img->_ref._lock);     //% unlock(img);  ???

	/* Data. Shared. */
	s = newseg(SG_DATA, t, (d-t)>>PGSHIFT);
	up->seg[DSEG] = s;

	/* Attached by hand */
	incref(& img->_ref);     //% incref(img);  
	s->image = img;
	s->fstart = ts->fstart+ts->flen;
	s->flen = data;

	/* BSS. Zero fill on demand */
	up->seg[BSEG] = newseg(SG_BSS, d, (b-d)>>PGSHIFT);

	/*
	 * Move the stack
	 */
	s = up->seg[ESEG];
	up->seg[ESEG] = 0;
	up->seg[SSEG] = s;
	qunlock(&up->seglock);
	poperror();	/* seglock */
	poperror();	/* elem */
	s->base = USTKTOP-USTKSIZE;
	s->top = USTKTOP;
	relocateseg(s, USTKTOP-TSTKTOP);

	/*
	 *  '/' processes are higher priority (hack to make /ip more responsive).
	 */
	if(devtab[tc->type]->dc == L'/')
		up->basepri = PriRoot;
	up->priority = up->basepri;
	poperror();
	cclose(tc);

	/*
	 *  At this point, the mmu contains info about the old address
	 *  space and needs to be flushed
	 */
	flushmmu();
	qlock(&up->debug);
	up->nnote = 0;
	up->notify = 0;
	up->notified = 0;
	up->privatemem = 0;
	procsetup(up);
	qunlock(&up->debug);
	if(up->hang)
		up->procctl = Proc_stopme;

	return execregs(entry, ssize, nargs);
	return 0;
}

long syssegbrk(ulong *arg) 
{
	int i;
	ulong addr;
	Segment *s;

	addr = arg[0];
	for(i = 0; i < NSEG; i++) {
		s = up->seg[i];
		if(s == 0 || addr < s->base || addr >= s->top)
			continue;
		switch(s->type&SG_TYPE) {
		case SG_TEXT:
		case SG_DATA:
		case SG_STACK:
			ERROR_RETURN(Ebadarg, ONERR);  //% error(Ebadarg);
		default:
			return ibrk(arg[1], i);
		}
	}
	ERROR_RETURN(Ebadarg, ONERR);  //% error(Ebadarg);
	return 0;		/* not reached */
}


long syssegattach(ulong *arg) //%
{
	return segattach(up, arg[0], (char*)arg[1], arg[2], arg[3]);
}


long syssegdetach(ulong *arg) 
{
	int i;
	ulong addr;
	Segment *s;

	qlock(&up->seglock);
	if(waserror()){
		qunlock(&up->seglock);
		nexterror();
	}

	s = 0;
	addr = arg[0];
	for(i = 0; i < NSEG; i++)
	        if( (s = up->seg[i]) ) {
			qlock(&s->lk);
			if((addr >= s->base && addr < s->top) ||
			   (s->top == s->base && addr == s->base))
				goto found;
			qunlock(&s->lk);
		}

	error(Ebadarg);

found:
	/*
	 * Check we are not detaching the initial stack segment.
	 */
	if(s == up->seg[SSEG]){
		qunlock(&s->lk);
		error(Ebadarg);
	}
	up->seg[i] = 0;
	qunlock(&s->lk);
	putseg(s);
	qunlock(&up->seglock);
	poperror();

	/* Ensure we flush any entries from the lost segment */
	flushmmu();
	return 0;
}


long syssegfree(ulong *arg) 
{
	Segment *s;
	ulong from, to;

	from = arg[0];
	s = seg(up, from, 1);
	if(s == nil)
		ERROR_RETURN(Ebadarg, ONERR);  //% error(Ebadarg);
	to = (from + arg[1]) & ~(BY2PG-1);
	from = PGROUND(from);

	if(to > s->top) {
		qunlock(&s->lk);
		ERROR_RETURN(Ebadarg, ONERR);  //% error(Ebadarg);
	}

	mfreeseg(s, from, (to - from) / BY2PG);
	qunlock(&s->lk);
	flushmmu();

	return 0;
}

#endif //------------------------------------------------------
