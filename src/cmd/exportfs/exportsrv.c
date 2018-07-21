//%%%%%%%%%%%%%
// (C) Bell Lab.  (C2) KM

#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#define Extern	extern
#include "exportfs.h"

#if 1 //%-------------------------------
#include  <l4all.h>

#define  DXX  if(0)print

extern void dbg9p(Fsrpc *f);
static L4_ThreadId_t  alloter;

static void __exits(char *s)
{
    print("  __exits(): Not Yet Implemented; \"%s\"\n", s);
}
#endif //%-------------------------------

char Ebadfid[] = "Bad fid";
char Enotdir[] = "Not a directory";
char Edupfid[] = "Fid already in use";
char Eopen[] = "Fid already opened";
char Exmnt[] = "Cannot .. past mount point";
char Emip[] = "Mount in progress";
char Enopsmt[] = "Out of pseudo mount points";
char Enomem[] = "No memory";
char Eversion[] = "Bad 9P2000 version";
char Ereadonly[] = "File system read only";

ulong messagesize;
int readonly;


void
Xversion(Fsrpc *t)
{
	Fcall rhdr;
DXX(">%s()\n", __FUNCTION__);
	if(t->work.msize > messagesize)
		t->work.msize = messagesize;
	messagesize = t->work.msize;
	if(strncmp(t->work.version, "9P2000", 6) != 0){
		reply(&t->work, &rhdr, Eversion);
		return;
	}
	rhdr.version = "9P2000";
	rhdr.msize = t->work.msize;
	reply(&t->work, &rhdr, 0);
	t->busy = 0;
}

void
Xauth(Fsrpc *t)
{
	Fcall rhdr;
DXX(">%s()\n", __FUNCTION__);
	reply(&t->work, &rhdr, "exportfs: authentication not required");
	t->busy = 0;
}

void
Xflush(Fsrpc *t)
{
	Fsrpc *w, *e;
	Fcall rhdr;
DXX(">%s()\n", __FUNCTION__);
	e = &Workq[Nr_workbufs];

	for(w = Workq; w < e; w++) {
		if(w->work.tag == t->work.oldtag) {
		        DEBUG(DFD, "\tQ busy %d pid %x can %d\n", w->busy, w->tid.raw, w->canint); 
			//%  <-   w->pid 
			if(w->busy && w->tid.raw) {   //%  <-  w->pid
				w->flushtag = t->work.tag;
				DEBUG(DFD, "\tset flushtag %d\n", t->work.tag);
				if(w->canint)
				  postnote(PNPROC, w->tid.raw, "flush"); //^ <- w->pid
				t->busy = 0;
				return;
			}
		}
	}

	reply(&t->work, &rhdr, 0);
	DEBUG(DFD, "\tflush reply\n");
	t->busy = 0;
}


void
Xattach(Fsrpc *t)
{
	int i, nfd;
	Fcall rhdr;
	Fid *f;
	char buf[128];
DXX(">%s()\n", __FUNCTION__);
	f = newfid(t->work.fid);
	if(f == 0) {
		reply(&t->work, &rhdr, Ebadfid);
		t->busy = 0;
		return;
	}

	if(srvfd >= 0){   // [-S srvfdfile]
		if(psmpt == 0){
		Nomount:
			reply(&t->work, &rhdr, Enopsmt);
			t->busy = 0;
			freefid(t->work.fid);
			return;
		}

		for(i=0; i<Npsmpt; i++)
			if(psmap[i] == 0)
				break;
		if(i >= Npsmpt)
			goto Nomount;

		sprint(buf, "%d", i);
		f->f = file(psmpt, buf);
		if(f->f == nil)
			goto Nomount;

		sprint(buf, "/mnt/exportfs/%d", i);
		nfd = dup(srvfd, -1);

		if(amount(nfd, buf, MREPL|MCREATE, t->work.aname) < 0){
			errstr(buf, sizeof buf);
			reply(&t->work, &rhdr, buf);
			t->busy = 0;
			freefid(t->work.fid);
			close(nfd);
			return;
		}

		psmap[i] = 1;
		f->mid = i;
	}
	else{
		f->f = root;
		f->f->ref++;
	}

	rhdr.qid = f->f->qid;
	reply(&t->work, &rhdr, 0);
	t->busy = 0;
}


Fid*
clonefid(Fid *f, int new)
{
	Fid *n;
DXX(">%s()\n", __FUNCTION__);
	n = newfid(new);
	if(n == 0) {
		n = getfid(new);
		if(n == 0)
			fatal("inconsistent fids");
		if(n->fid >= 0)
			close(n->fid);
		freefid(new);
		n = newfid(new);
		if(n == 0)
			fatal("inconsistent fids2");
	}
	n->f = f->f;
	n->f->ref++;
	return n;
}


void
Xwalk(Fsrpc *t)
{
	char err[ERRMAX], *e;
	Fcall rhdr;
	Fid *f, *nf;
	File *wf;
	int i;
DXX(">%s()\n", __FUNCTION__);
	f = getfid(t->work.fid);
	if(f == 0) {
		reply(&t->work, &rhdr, Ebadfid);
		t->busy = 0;
		return;
	}

	nf = nil;
	if(t->work.newfid != t->work.fid){
		nf = clonefid(f, t->work.newfid);
		f = nf;
	}

	rhdr.nwqid = 0;
	e = nil;
	for(i=0; i<t->work.nwname; i++){
		if(i == MAXWELEM){
			e = "Too many path elements";
			break;
		}

		if(strcmp(t->work.wname[i], "..") == 0) {
			if(f->f->parent == nil) {
				e = Exmnt;
				break;
			}
			wf = f->f->parent;
			wf->ref++;
			goto Accept;
		}
	
		wf = file(f->f, t->work.wname[i]);
		if(wf == 0){
			errstr(err, sizeof err);
			e = err;
			break;
		}
    Accept:
		freefile(f->f);
		rhdr.wqid[rhdr.nwqid++] = wf->qid;
		f->f = wf;
		continue;
	}

	if(nf!=nil && (e!=nil || rhdr.nwqid!=t->work.nwname))
		freefid(t->work.newfid);
	if(rhdr.nwqid > 0)
		e = nil;
	reply(&t->work, &rhdr, e);
	t->busy = 0;
}

void
Xclunk(Fsrpc *t)
{
	Fcall rhdr;
	Fid *f;
DXX(">%s()\n", __FUNCTION__);
	f = getfid(t->work.fid);
	if(f == 0) {
		reply(&t->work, &rhdr, Ebadfid);
		t->busy = 0;
		return;
	}

	if(f->fid >= 0)
		close(f->fid);

	freefid(t->work.fid);
	reply(&t->work, &rhdr, 0);
	t->busy = 0;
}

void
Xstat(Fsrpc *t)
{
	char err[ERRMAX], *path;
	Fcall rhdr;
	Fid *f;
	Dir *d;
	int s;
	uchar *statbuf;
DXX(">%s()\n", __FUNCTION__);
	f = getfid(t->work.fid);
	if(f == 0) {
		reply(&t->work, &rhdr, Ebadfid);
		t->busy = 0;
		return;
	}

	if(f->fid >= 0)
		d = dirfstat(f->fid);
	else {
		path = makepath(f->f, "");
		d = dirstat(path);
		free(path);
	}

	if(d == nil) {
		errstr(err, sizeof err);
		reply(&t->work, &rhdr, err);
		t->busy = 0;
		return;
	}

	d->qid.path = f->f->qidt->uniqpath;

	s = sizeD2M(d);
	statbuf = emallocz(s);
	s = convD2M(d, statbuf, s);
	free(d);
	rhdr.nstat = s;
	rhdr.stat = statbuf;
	reply(&t->work, &rhdr, 0);
	free(statbuf);
	t->busy = 0;
}

static int
getiounit(int fd)
{
	int n;
DXX(">%s()\n", __FUNCTION__);
	n = iounit(fd);
	if(n > messagesize-IOHDRSZ)
		n = messagesize-IOHDRSZ;
	return n;
}

void
Xcreate(Fsrpc *t)
{
	char err[ERRMAX], *path;
	Fcall rhdr;
	Fid *f;
	File *nf;
DXX(">%s()\n", __FUNCTION__);
	if(readonly) {
		reply(&t->work, &rhdr, Ereadonly);
		t->busy = 0;
		return;
	}
	f = getfid(t->work.fid);
	if(f == 0) {
		reply(&t->work, &rhdr, Ebadfid);
		t->busy = 0;
		return;
	}
	

	path = makepath(f->f, t->work.name);
	f->fid = create(path, t->work.mode, t->work.perm);
	free(path);
	if(f->fid < 0) {
		errstr(err, sizeof err);
		reply(&t->work, &rhdr, err);
		t->busy = 0;
		return;
	}

	nf = file(f->f, t->work.name);
	if(nf == 0) {
		errstr(err, sizeof err);
		reply(&t->work, &rhdr, err);
		t->busy = 0;
		return;
	}

	f->mode = t->work.mode;
	freefile(f->f);
	f->f = nf;
	rhdr.qid = f->f->qid;
	rhdr.iounit = getiounit(f->fid);
	reply(&t->work, &rhdr, 0);
	t->busy = 0;
}

void
Xremove(Fsrpc *t)
{
	char err[ERRMAX], *path;
	Fcall rhdr;
	Fid *f;
DXX(">%s()\n", __FUNCTION__);
	if(readonly) {
		reply(&t->work, &rhdr, Ereadonly);
		t->busy = 0;
		return;
	}
	f = getfid(t->work.fid);
	if(f == 0) {
		reply(&t->work, &rhdr, Ebadfid);
		t->busy = 0;
		return;
	}

	path = makepath(f->f, "");
	DEBUG(DFD, "\tremove: %s\n", path);
	if(remove(path) < 0) {
		free(path);
		errstr(err, sizeof err);
		reply(&t->work, &rhdr, err);
		t->busy = 0;
		return;
	}
	free(path);

	f->f->inval = 1;
	if(f->fid >= 0)
		close(f->fid);
	freefid(t->work.fid);

	reply(&t->work, &rhdr, 0);
	t->busy = 0;
}

void
Xwstat(Fsrpc *t)
{
	char err[ERRMAX], *path;
	Fcall rhdr;
	Fid *f;
	int s;
	char *strings;
	Dir d;
DXX(">%s()\n", __FUNCTION__);
	if(readonly) {
		reply(&t->work, &rhdr, Ereadonly);
		t->busy = 0;
		return;
	}

	f = getfid(t->work.fid);
	if(f == 0) {
		reply(&t->work, &rhdr, Ebadfid);
		t->busy = 0;
		return;
	}

	strings = emallocz(t->work.nstat);	/* ample */
	if(convM2D(t->work.stat, t->work.nstat, &d, strings) <= BIT16SZ){
		rerrstr(err, sizeof err);
		reply(&t->work, &rhdr, err);
		t->busy = 0;
		free(strings);
		return;
	}

	if(f->fid >= 0)
		s = dirfwstat(f->fid, &d);
	else {
		path = makepath(f->f, "");
		s = dirwstat(path, &d);
		free(path);
	}

	if(s < 0) {
		rerrstr(err, sizeof err);
		reply(&t->work, &rhdr, err);
	}
	else {
		/* wstat may really be rename */
		if(strcmp(d.name, f->f->name)!=0 && strcmp(d.name, "")!=0){
			free(f->f->name);
			f->f->name = estrdup(d.name);
		}
		reply(&t->work, &rhdr, 0);
	}
	free(strings);
	t->busy = 0;
}


extern  L4_ThreadId_t create_start_thread(uint , uint );

void
slave(Fsrpc *f)
{
	Proc *p;
	//uintptr pid;
	Fcall rhdr;
	static int nproc;
DXX(">%s()\n", __FUNCTION__);

	if(readonly){
		switch(f->work.type){
		case Twrite:
			reply(&f->work, &rhdr, Ereadonly);
			f->busy = 0;
			return;

		case Topen:
		  	if((f->work.mode&3) == OWRITE || (f->work.mode&OTRUNC)){
				reply(&f->work, &rhdr, Ereadonly);
				f->busy = 0;
				return;
			}
		}
	}

#if 1 //%%--- Multi thread server----------------------------------------
	alloter = L4_Myself();
	//print("EXPSRV <%x  %x>\n", alloter.raw, L4_MyLocalId().raw); 

	for(;;) {
		for(p = Proclist; p; p = p->next) {
			if(p->busy == 0) {
			        L4_MsgTag_t  tag;
				//	dbg9p(&f->work); 
				
				f->proc = p;
				f->tid = p->tid;
				p->busy = 1;
				L4_LoadMR(0, 1);
				L4_LoadMR(1, (uint)f);
				tag = L4_Send(p->tid);
				if (L4_IpcFailed(tag)) { 
				      print("  exportfs:slave:send:ErrCode:%d\n", L4_ErrorCode());
				}
				return;
			}	
		}

		if(++nproc > MAXPROC)
			fatal("too many threads");

		{  // create a new worker thread
		    uint  *stk = (uint*)malloc(4096);
		    uint  *sp  = &stk[1020];   

		    p = malloc(sizeof(Proc));
		    if (p == 0)
		        fatal("out of memory");
 
		    p->tid = create_start_thread((unsigned)blockingslave, (unsigned)sp);
		    sleep(2);
		    p->busy = 0;
		    p->next = Proclist;
		    Proclist = p;
		}
	}
#else  //original-----------------------------------------
	for(;;) {
		for(p = Proclist; p; p = p->next) {
			if(p->busy == 0) {
				f->pid = p->pid;
				p->busy = 1;
				pid = (uintptr)rendezvous((void*)p->pid, f);
				if(pid != p->pid)
					fatal("rendezvous sync fail");
				return;
			}	
		}

		if(++nproc > MAXPROC)
			fatal("too many procs");

		pid = rfork(RFPROC|RFMEM);
		switch(pid) {
		case -1:
			fatal("rfork");

		case 0:
			blockingslave();
			fatal("slave");

		default:
			p = malloc(sizeof(Proc));
			if(p == 0)
				fatal("out of memory");

			p->busy = 0;
			p->pid = pid;
			p->next = Proclist;
			Proclist = p;

			rendezvous((void*)pid, p);
		}
	}
#endif //%%------------------------------------------------
}


#if 1 //%------------------------------------------------
void
blockingslave(void)
{
	Fsrpc *p;
	Fcall rhdr;
	Proc *m;
	//uintptr pid;
	L4_ThreadId_t   sender;
DXX(">%s()\n", __FUNCTION__);

	L4_MsgTag_t  tag;   //% pid and m must be cared !!!
	L4_LoadBR(0, 0);
	
	for(;;) {
	        //tag = L4_Receive(alloter);
		tag = L4_Wait(&sender);
		if (L4_IpcFailed(tag)) {     
		        print("exportsrv:blockingslave:ErrCode:%d\n", L4_ErrorCode());
			continue;
		}
		if (!L4_SameThreads(sender, alloter)) {
		    print("    blockingslave:recv(%x : %x)\n", sender.raw, alloter.raw);
		    continue;
		}

		L4_StoreMR(1, (L4_Word_t*)&p);
		//DEBUG(DFD, "\tslave: %p %F b %d p %x\n", pid, &p->work, p->busy, p->tid.raw);
		m = p->proc;

		sleep(1);
		//dbg9p(&p->work); 

		if(p->flushtag != NOTAG)
			goto flushme;

		switch(p->work.type) {
		case Tread:
			slaveread(p);
			break;

		case Twrite:
			slavewrite(p);
			break;

		case Topen:
			slaveopen(p);
			break;

		default:
			reply(&p->work, &rhdr, "exportfs: slave type error");
		}

		if(p->flushtag != NOTAG) {
flushme:
			p->work.type = Tflush;
			p->work.tag = p->flushtag;
			reply(&p->work, &rhdr, 0);
		}
		p->busy = 0;
		m->busy = 0;
	}
}
#else //%plan9 -----------------------------------------------
void
blockingslave(void)
{
	Fsrpc *p;
	Fcall rhdr;
	Proc *m;
	uintptr pid;

	notify(flushaction);
	pid = getpid();
	m = rendezvous((void*)pid, 0);
	
	for(;;) {
		p = rendezvous((void*)pid, (void*)pid);
		if(p == (void*)~0)			/* Interrupted */
			continue;
		DEBUG(DFD, "\tslave: %p %F b %d p %p\n", pid, &p->work, p->busy, p->pid);

		if(p->flushtag != NOTAG)
			goto flushme;

		switch(p->work.type) {
		case Tread:
			slaveread(p);
			break;

		case Twrite:
			slavewrite(p);
			break;

		case Topen:
			slaveopen(p);
			break;

		default:
			reply(&p->work, &rhdr, "exportfs: slave type error");
		}

		if(p->flushtag != NOTAG) {
flushme:
			p->work.type = Tflush;
			p->work.tag = p->flushtag;
			reply(&p->work, &rhdr, 0);
		}
		p->busy = 0;
		m->busy = 0;
	}
}
#endif //%----------------------------------------------------


int
openmount(int sfd)
{
	int p[2];
	char *arg[10], fdbuf[20], mbuf[20];
DXX(">%s()\n", __FUNCTION__);
	if(pipe(p) < 0)
		return -1;

	switch(rfork(RFPROC|RFMEM|RFNOWAIT|RFNAMEG|RFFDG)){
	case -1:
		return -1;

	default:
		close(sfd);
		close(p[0]);
		return p[1];

	case 0:
		break;
	}

	close(p[1]);

	arg[0] = "exportfs";
	snprint(fdbuf, sizeof fdbuf, "-S/fd/%d", sfd);
	arg[1] = fdbuf;
	snprint(mbuf, sizeof mbuf, "-m%lud", messagesize-IOHDRSZ);
	arg[2] = mbuf;
	arg[3] = nil;

	close(0);
	close(1);
	dup(p[0], 0);
	dup(p[0], 1);
print("  exportsrv:openmount:exec\n");
        exec("/t/bin/exportfs", arg);  //%
	//%original	exec("/bin/exportfs", arg);
	__exits("whoops: exec failed");	
	return -1;
}


void
slaveopen(Fsrpc *p)
{
	char err[ERRMAX], *path;
	Fcall *work, rhdr;
	Fid *f;
	Dir *d;

	work = &p->work;
//fmtinstall('F', fcallfmt);
DXX(">%s(Fsrpc*) ", __FUNCTION__);
//dbg9p(work);

	f = getfid(work->fid);
	if(f == 0) {
		reply(work, &rhdr, Ebadfid);
		return;
	}

	if(f->fid >= 0) {
		close(f->fid);
		f->fid = -1;
	}
	
	path = makepath(f->f, "");
	DEBUG(DFD, "\topen: %s %d\n", path, work->mode);

	p->canint = 1;
	if(p->flushtag != NOTAG){
		free(path);
		return;
	}

	/* There is a race here I ignore because there are no locks */
	f->fid = open(path, work->mode);

	free(path);
	p->canint = 0;

	if(f->fid < 0 || (d = dirfstat(f->fid)) == nil) {
	Error:
		errstr(err, sizeof err);
		reply(work, &rhdr, err);
		return;
	}

	f->f->qid = d->qid;
	free(d);

	if(f->f->qid.type & QTMOUNT){	/* fork new exportfs for this */
		f->fid = openmount(f->fid);
		if(f->fid < 0)
			goto Error;
	}

	DEBUG(DFD, "\topen: fd %d\n", f->fid);
	f->mode = work->mode;
	f->offset = 0;
	rhdr.iounit = 8192;  //%%%
	//%	rhdr.iounit = getiounit(f->fid);
	rhdr.qid = f->f->qid;
	reply(work, &rhdr, 0);
DXX("<%s()\n", __FUNCTION__);
}


void
slaveread(Fsrpc *p)
{
	Fid *f;
	int n, r;
	Fcall *work, rhdr;
	char *data, err[ERRMAX];
DXX(">%s() ", __FUNCTION__);
	work = &p->work;

	f = getfid(work->fid);
	if(f == 0) {
		reply(work, &rhdr, Ebadfid);
		return;
	}

	n = (work->count > messagesize-IOHDRSZ) ? messagesize-IOHDRSZ : work->count;
	p->canint = 1;
	if(p->flushtag != NOTAG)
		return;

	data = malloc(n);
	if(data == nil)
		fatal(Enomem);

DXX("--------slaveread-1(%d) ", f->fid);
	/* can't just call pread, since directories must update the offset */
	if(patternfile != nil && (f->f->qid.type&QTDIR)){
		r = preaddir(f, (uchar*)data, n, work->offset);
DXX("slave:preaddir(%d, %x, %d, %d):%d\n", f->fid, data, n, (int)work->offset, r);
	}
	else{
		r = pread(f->fid, data, n, work->offset);
DXX("slave:pread(%d, %x, %d, %d):%d\n", f->fid, data, n, (int)work->offset, r);
	}

	p->canint = 0;
	if(r < 0) {
		free(data);
		errstr(err, sizeof err);
		reply(work, &rhdr, err);
		return;
	}

	DEBUG(DFD, "\tread: fd=%d %d bytes\n", f->fid, r);
DXX("--------slaveread(%d):%d\n", f->fid, r);

	rhdr.data = data;
	rhdr.count = r;
	reply(work, &rhdr, 0);
	free(data);
}

void
slavewrite(Fsrpc *p)
{
	char err[ERRMAX];
	Fcall *work, rhdr;
	Fid *f;
	int n;
DXX(">%s()\n", __FUNCTION__);
	work = &p->work;

	f = getfid(work->fid);
	if(f == 0) {
		reply(work, &rhdr, Ebadfid);
		return;
	}

	n = (work->count > messagesize-IOHDRSZ) ? messagesize-IOHDRSZ : work->count;
	p->canint = 1;
	if(p->flushtag != NOTAG)
		return;

	n = pwrite(f->fid, work->data, n, work->offset);
	p->canint = 0;
	if(n < 0) {
		errstr(err, sizeof err);
		reply(work, &rhdr, err);
		return;
	}

	DEBUG(DFD, "\twrite: %d bytes fd=%d\n", n, f->fid);

	rhdr.count = n;
	reply(work, &rhdr, 0);
}

void
reopen(Fid *f)
{
        //%	USED(f);
	fatal("reopen");
}

void
flushaction(void *a, char *cause)
{
DXX(">%s()\n", __FUNCTION__);
        //%  USED(a);
	if(strncmp(cause, "sys:", 4) == 0 && !strstr(cause, "pipe")) {
		fprint(2, "exportsrv: note: %s\n", cause);
		exits("noted");
	}
	if(strncmp(cause, "kill", 4) == 0)
		noted(NDFLT);

	noted(NCONT);
}
