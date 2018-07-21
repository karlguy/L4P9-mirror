//%%%%%%%%%  ramfs.c : Example server code %%%%%%%%%%%%%

#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>

#if 1 //%------------------------------------------------------
#include  <l4all.h>
#include  <l4/l4io.h>
#include <lp49/lp49.h>

#define   _DBGFLG  0
#include  <l4/_dbgpr.h>
#define  PRN  l4printf_b


extern int l4printf_s(const char *, ...);

int getpid( ) { return TID2PROCNR(L4_Myself());  }  // devproc 

static void mount_thread();
// static added.
#endif //%----------------------------------------------------


/*
 * Rather than reading /adm/users, which is a lot of work for
 * a toy program, we assume all groups have the form
 *	NNN:user:user:
 * meaning that each user is the leader of his own group.
 */

enum
{
	OPERM	= 0x3,		/* mask of all permission types in open mode */
	Nram	= 2048,
	Maxsize	= 768*1024*1024,
	Maxfdata	= 8192,
};

typedef struct Fid Fid;
typedef struct Ram Ram;

struct Fid
{
	short	busy;
	short	open;
	short	rclose;
	int	fid;
	Fid	*next;
	char	*user;
	Ram	*ram;
};

struct Ram
{
	short	busy;
	short	open;
	long	parent;		/* index in Ram array */
	Qid	qid;
	long	perm;
	char	*name;
	ulong	atime;
	ulong	mtime;
	char	*user;
	char	*group;
	char	*muid;
	char	*data;
	long	ndata;
};

enum
{
	Pexec =		1,
	Pwrite = 	2,
	Pread = 	4,
	Pother = 	1,
	Pgroup = 	8,
	Powner =	64,
};


static  ulong	path;		/* incremented for each new file */
static  Fid	*fids;
static  Ram	ram[Nram];
static  int	nram;
static  int	mfd[2];
static  char	*user;
static  uchar	mdata[IOHDRSZ+Maxfdata];
static  uchar	rdata[Maxfdata];	/* buffer for data in reply */
static  uchar   statbuf[STATMAX];
static  Fcall   thdr;
static  Fcall	rhdr;
static  int	messagesize = sizeof mdata;

static  Fid *	newfid(int);
static  uint	ramstat(Ram*, uchar*, uint);
static  void	error(char*);
static  void	io(void);
static  void	*erealloc(void*, ulong);
static  void	*emalloc(ulong);
static  char	*estrdup(char*);
static  void	usage(void);
static  int	perm(Fid*, Ram*, int);

static  char	*rflush(Fid*), *rversion(Fid*), *rauth(Fid*),
	*rattach(Fid*), *rwalk(Fid*),
	*ropen(Fid*), *rcreate(Fid*),
	*rread(Fid*), *rwrite(Fid*), *rclunk(Fid*),
	*rremove(Fid*), *rstat(Fid*), *rwstat(Fid*);

static  int needfid[] = {
	[Tversion] 0,
	[Tflush] 0,
	[Tauth] 0,
	[Tattach] 0,
	[Twalk] 1,
	[Topen] 1,
	[Tcreate] 1,
	[Tread] 1,
	[Twrite] 1,
	[Tclunk] 1,
	[Tremove] 1,
	[Tstat] 1,
	[Twstat] 1,
};


static  char 	*(*fcalls[])(Fid*) = {
	[Tversion]	rversion,
	[Tflush]	rflush,
	[Tauth]	        rauth,
	[Tattach]	rattach,
	[Twalk]		rwalk,
	[Topen]		ropen,
	[Tcreate]	rcreate,
	[Tread]		rread,
	[Twrite]	rwrite,
	[Tclunk]	rclunk,
	[Tremove]	rremove,
	[Tstat]		rstat,
	[Twstat]	rwstat,
};


static  char	Eperm[] =	"permission denied";
static  char	Enotdir[] =	"not a directory";
static  char	Enoauth[] =	"ramfs: authentication not required";
static  char	Enotexist[] =	"file not exist"; //%
static  char	Einuse[] =	"file in use";
static  char	Eexist[] =	"file exists";
static  char	Eisdir[] =	"file is a directory";
static  char	Enotowner[] =	"not owner";
static  char	Eisopen[] = 	"file already open for I/O";
static  char	Excl[] = 	"exclusive use file already open";
static  char	Ename[] = 	"illegal name";
static  char	Eversion[] =	"unknown 9P version";
static  char	Enotempty[] =	"directory not empty";

static  int debug = 0;
static  int private;


static  void
notifyf(void *a, char *s)
{
        DBGPRN(">%s() ", __FUNCTION__);
	//%  USED(a);
	if(strncmp(s, "interrupt", 9) == 0)
		noted(NCONT);
	noted(NDFLT);
}

//===========================================

static char *defmnt = 0;
static int   p[2];


void
main(int argc, char *argv[])
{
	Ram *r;
	//%     char *defmnt = 0;
	//%	int p[2];
	int fd;
	int stdio = 0;
	char *service;
	int  i;

#if 0 //%------------------------------------------------
	extern void wait_preproc(int); 
	wait_preproc(0);

	PRN(">ramfs:main( ) \n");
	service = "ramfs";
        defmnt = "/tmp";
	debug = 0;
#else  //%----------------------------------------------
	service = "ramfs";
        defmnt = "/tmp";

	ARGBEGIN{
	case 'D':
		debug = 1;
		break;
	case 'i':
		defmnt = 0;
		stdio = 1;
		mfd[0] = 0;
		mfd[1] = 1;
		break;
	case 's':
		defmnt = 0;
		break;
	case 'm':
		defmnt = EARGF(usage());
		break;
	case 'p':
		private++;
		break;
	case 'S':
		defmnt = 0;
		service = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND	;
#endif  //%--------------------------------------------------------

	// l4printf_s("debug=%d defmnt=%s stdio=%d service=%s private=%d\n",
	//	 debug, defmnt, stdio, service, private);

	/* 
	 * default-option ==>  mount the pipe p[1] onto /tmp. 
	 *        defmnt = "/tmp";       
	 * -s option      ==>  Post the pipe p[1] on /srv/ramfs 
	 *        defmnt = 0;    service = "ramfs";      
	 * -S XXX option  ==> Post the pipe p[1] on /srv/XXX
	 */

	if(pipe(p) < 0)
		error("ramfs: pipe failed");
	DBGPRN(">ramfs:pipe[0]=%d pipe[1]=%d ", p[0], p[1]);

	if(!stdio){
		mfd[0] = p[0];
		mfd[1] = p[0];
		if(defmnt == 0){
			char buf[64];
			snprint(buf, sizeof buf, "#s/%s", service);
			fd = create(buf, OWRITE|ORCLOSE, 0666);
			if(fd < 0)
				error("create failed");

			L4_Yield();
			sprint(buf, "%d", p[1]);
			if(write(fd, buf, strlen(buf)) < 0)
				error("writing service file");

			PRN("ramfs starting: \"/srv/%s\" pipe<%d %d> debug=%d \n", 
			    service, p[0], p[1], debug);
		}
	}

	user = getuser();
	//%	notify(notifyf);
	nram = 1;
	r = &ram[0];
	r->busy = 1;
	r->data = 0;
	r->ndata = 0;
	r->perm = DMDIR | 0775;
	r->qid.type = QTDIR;
	r->qid.path = 0LL;
	r->qid.vers = 0;
	r->parent = 0;
	r->user = user;
	r->group = user;
	r->muid = user;
	r->atime = time(0);
	r->mtime = r->atime;
	r->name = estrdup(".");

	PRN(">ramfs starting... ");

	if(debug)
		fmtinstall('F', fcallfmt);

#if 1 //%-------------------------------------------
	if (defmnt)  //  /tmp 
        {
	    static unsigned  mystack[128];
	    L4_ThreadId_t    tid;

	    extern L4_ThreadId_t create_start_thread(unsigned pc, unsigned stkp);
	    tid = create_start_thread((unsigned)mount_thread, &mystack[120]);

	    io();
	}
	else {   // -s option:  /srv/ramfs  ----------------------

	    extern post_nextproc(int);
	    io( );
	    close(mfd[0]);
	    close(mfd[1]);
	}

#else //original-----------------------------------
	switch(rfork(RFFDG|RFPROC|RFNAMEG|RFNOTEG)){
	case -1:
		error("fork");
	case 0:
		close(p[1]);
		io();
		break;
	default:
		close(p[0]);	/* don't deadlock if child fails */
		if(defmnt && mount(p[1], -1, defmnt, MREPL | MCREATE, "") < 0)
			error("mount failed");
	}
	exits(0);
#endif //%------------------------------------------
}


//%
static  void mount_thread()
{
    L4_Yield();
    L4_Sleep(L4_TimePeriod(1000000));
    DBGPRN(">%s() ", __FUNCTION__);

    if(defmnt && mount(p[1], -1, defmnt, /*MREPL*/ MAFTER | MCREATE, "") < 0)
        error("mount failed");

    L4_Sleep(L4_Never);
}


static  char*
rversion(Fid* _x)
{
	Fid *f;
        DBGPRN(">%s() version:\"%s\" ", __FUNCTION__, thdr.version);

	for(f = fids; f; f = f->next)
		if(f->busy)
			rclunk(f);
	if(thdr.msize > sizeof mdata)
		rhdr.msize = sizeof mdata;
	else
		rhdr.msize = thdr.msize;
	messagesize = rhdr.msize;
	if(strncmp(thdr.version, "9P2000", 6) != 0)
		return Eversion;
	rhdr.version = "9P2000";

	return 0;
}


static  char*
rauth(Fid* _x)
{
        DBGPRN(">%s() uname:\"%s\" ", __FUNCTION__, thdr.uname);
	return "ramfs: no authentication required";
}

static  char*
rflush(Fid *f)
{
        //%  USED(f);
	return 0;
}

static  char*
rattach(Fid *f)
{
	/* no authentication! */
        DBGPRN(">%s() ", __FUNCTION__);

	f->busy = 1;
	f->rclose = 0;
	f->ram = &ram[0];
	rhdr.qid = f->ram->qid;
	if(thdr.uname[0])
		f->user = estrdup(thdr.uname);
	else
		f->user = "none";
	if(strcmp(user, "none") == 0)
		user = f->user;

	return 0;
}

static  char*
clone(Fid *f, Fid **nf)
{
        DBGPRN(">%s() ", __FUNCTION__);

	if(f->open)
		return Eisopen;
	if(f->ram->busy == 0)
		return Enotexist;
	*nf = newfid(thdr.newfid);
	(*nf)->busy = 1;
	(*nf)->open = 0;
	(*nf)->rclose = 0;
	(*nf)->ram = f->ram;
	(*nf)->user = f->user;	/* no ref count; the leakage is minor */

	return 0;
}

static  char*
rwalk(Fid *f)
{
	Ram *r, *fram;
	char *name;
	Ram *parent;
	Fid *nf;
	char *err;
	ulong t;
	int i;

        DBGPRN(">%s(nwname:%d wname:\"%s\") ", 
	       __FUNCTION__, thdr.nwname, thdr.wname[0]);

	err = nil;
	nf = nil;
	rhdr.nwqid = 0;
	if(thdr.newfid != thdr.fid){
	        err = clone(f, &nf);    //?????
		if(err)
			return err;
		f = nf;	/* walk the new fid */
	}

	fram = f->ram;
	if(thdr.nwname > 0){
		t = time(0);
		for(i=0; i<thdr.nwname && i<MAXWELEM; i++){
			if((fram->qid.type & QTDIR) == 0){
				err = Enotdir;
 				break;
			}
			if(fram->busy == 0){
				err = Enotexist;
				break;
			}
			fram->atime = t;
			name = thdr.wname[i];
			if(strcmp(name, ".") == 0){
    Found:
				rhdr.nwqid++;
				rhdr.wqid[i] = fram->qid;
				continue;
			}
			parent = &ram[fram->parent];
			if(!perm(f, parent, Pexec)){
				err = Eperm;
				break;
			}
			if(strcmp(name, "..") == 0){
				fram = parent;
				goto Found;
			}
			for(r=ram; r < &ram[nram]; r++)
				if(r->busy && r->parent==fram-ram && strcmp(name, r->name)==0){
					fram = r;
					goto Found;
				}
			break;
		}
		if(i==0 && err == nil)
			err = Enotexist;
	}
	if(nf != nil && (err!=nil || rhdr.nwqid<thdr.nwname)){
		/* clunk the new fid, which is the one we walked */
		f->busy = 0;
		f->ram = nil;
	}
	if(rhdr.nwqid > 0)
		err = nil;	/* didn't get everything in 9P2000 right! */
	if(rhdr.nwqid == thdr.nwname)	/* update the fid after a successful walk */
		f->ram = fram;

	return err;
}


static  char *
ropen(Fid *f)
{
	Ram *r;
	int mode, trunc;

        DBGPRN(">%s(name:\"%s\") ", __FUNCTION__, thdr.name);

	if(f->open)
		return Eisopen;
	r = f->ram;
	if(r->busy == 0)
		return Enotexist;
	if(r->perm & DMEXCL)
		if(r->open)
			return Excl;
	mode = thdr.mode;
	if(r->qid.type & QTDIR){
		if(mode != OREAD)
			return Eperm;
		rhdr.qid = r->qid;
		return 0;
	}
	if(mode & ORCLOSE){
		/* can't remove root; must be able to write parent */
		if(r->qid.path==0 || !perm(f, &ram[r->parent], Pwrite))
			return Eperm;
		f->rclose = 1;
	}
	trunc = mode & OTRUNC;
	mode &= OPERM;
	if(mode==OWRITE || mode==ORDWR || trunc)
		if(!perm(f, r, Pwrite))
			return Eperm;
	if(mode==OREAD || mode==ORDWR)
		if(!perm(f, r, Pread))
			return Eperm;
#if 0 //% To allow program on DOS file run
	if(mode==OEXEC)
		if(!perm(f, r, Pexec))
			return Eperm;
#endif //%
	if(trunc && (r->perm&DMAPPEND)==0){
		r->ndata = 0;
		if(r->data)
			free(r->data);
		r->data = 0;
		r->qid.vers++;
	}
	rhdr.qid = r->qid;
	rhdr.iounit = messagesize-IOHDRSZ;
	f->open = 1;
	r->open++;

	return 0;
}

static  char *
rcreate(Fid *f)
{
	Ram *r;
	char *name;
	long parent, prm;
        DBGPRN(">%s(name:\"%s\" perm:%x mode:%x) ", 
	       __FUNCTION__, thdr.name, thdr.perm, thdr.mode);

	if(f->open)
		return Eisopen;
	if(f->ram->busy == 0)
		return Enotexist;
	parent = f->ram - ram;
	if((f->ram->qid.type&QTDIR) == 0)
		return Enotdir;
	/* must be able to write parent */
	//% if(!perm(f, f->ram, Pwrite))
	//%	return Eperm;
	prm = thdr.perm;
	name = thdr.name;
	if(strcmp(name, ".")==0 || strcmp(name, "..")==0)
		return Ename;
	for(r=ram; r<&ram[nram]; r++)
		if(r->busy && parent==r->parent)
		if(strcmp((char*)name, r->name)==0)
			return Einuse;
	for(r=ram; r->busy; r++)
		if(r == &ram[Nram-1])
			return "no free ram resources";
	r->busy = 1;
	r->qid.path = ++path;
	r->qid.vers = 0;
	if(prm & DMDIR)
		r->qid.type |= QTDIR;
	r->parent = parent;
	free(r->name);
	r->name = estrdup(name);
	r->user = f->user;
	r->group = f->ram->group;
	r->muid = f->ram->muid;
	if(prm & DMDIR)
		prm = (prm&~0777) | (f->ram->perm&prm&0777);
	else
		prm = (prm&(~0777|0111)) | (f->ram->perm&prm&0666);
	r->perm = prm;
	r->ndata = 0;
	if(r-ram >= nram)
		nram = r - ram + 1;
	r->atime = time(0);
	r->mtime = r->atime;
	f->ram->mtime = r->atime;
	f->ram = r;
	rhdr.qid = r->qid;
	rhdr.iounit = messagesize-IOHDRSZ;
	f->open = 1;
	if(thdr.mode & ORCLOSE)
		f->rclose = 1;
	r->open++;

	return 0;
}

static  char*
rread(Fid *f)
{
	Ram *r;
	uchar *buf;
	long off;
	int n, m, cnt;

        DBGPRN(">%s() ", __FUNCTION__);

	if(f->ram->busy == 0)
		return Enotexist;
	n = 0;
	rhdr.count = 0;
	off = thdr.offset;
	buf = rdata;
	cnt = thdr.count;
	if(cnt > messagesize)	/* shouldn't happen, anyway */
		cnt = messagesize;
	if(f->ram->qid.type & QTDIR){
		for(r=ram+1; off > 0; r++){
			if(r->busy && r->parent==f->ram-ram)
				off -= ramstat(r, statbuf, sizeof statbuf);
			if(r == &ram[nram-1])
				return 0;
		}
		for(; r<&ram[nram] && n < cnt; r++){
			if(!r->busy || r->parent!=f->ram-ram)
				continue;
			m = ramstat(r, buf+n, cnt-n);
			if(m == 0)
				break;
			n += m;
		}
		rhdr.data = (char*)rdata;
		rhdr.count = n;
		return 0;
	}
	r = f->ram;
	if(off >= r->ndata)
		return 0;
	r->atime = time(0);
	n = cnt;
	if(off+n > r->ndata)
		n = r->ndata - off;
	rhdr.data = r->data+off;
	rhdr.count = n;

	return 0;
}

static  char*
rwrite(Fid *f)
{
	Ram *r;
	ulong off;
	int cnt;
        DBGPRN(">%s() ", __FUNCTION__);

	r = f->ram;
	if(r->busy == 0)
		return Enotexist;
	off = thdr.offset;
	if(r->perm & DMAPPEND)
		off = r->ndata;
	cnt = thdr.count;
	if(r->qid.type & QTDIR)
		return Eisdir;
	if(off+cnt >= Maxsize)		/* sanity check */
		return "write too big";
	if(off+cnt > r->ndata)
		r->data = erealloc(r->data, off+cnt);
	if(off > r->ndata)
		memset(r->data+r->ndata, 0, off-r->ndata);
	if(off+cnt > r->ndata)
		r->ndata = off+cnt;
	memmove(r->data+off, thdr.data, cnt);
	r->qid.vers++;
	r->mtime = time(0);
	rhdr.count = cnt;

	return 0;
}


static int
emptydir(Ram *dr)
{
	long didx = dr - ram;
	Ram *r;

        DBGPRN(">%s() ", __FUNCTION__);

	for(r=ram; r<&ram[nram]; r++)
		if(r->busy && didx==r->parent)
			return 0;
	return 1;
}

static  char *
realremove(Ram *r)
{
        DBGPRN(">%s() ", __FUNCTION__);

	if(r->qid.type & QTDIR && !emptydir(r))
		return Enotempty;
	r->ndata = 0;
	if(r->data)
		free(r->data);
	r->data = 0;
	r->parent = 0;
	memset(&r->qid, 0, sizeof r->qid);
	free(r->name);
	r->name = nil;
	r->busy = 0;

	return nil;
}

static  char *
rclunk(Fid *f)
{
	char *e = nil;

        DBGPRN(">%s() ", __FUNCTION__);

	if(f->open)
		f->ram->open--;
	if(f->rclose)
		e = realremove(f->ram);
	f->busy = 0;
	f->open = 0;
	f->ram = 0;

	return e;
}

static  char *
rremove(Fid *f)
{
	Ram *r;

        DBGPRN(">%s() ", __FUNCTION__);

	if(f->open)
		f->ram->open--;
	f->busy = 0;
	f->open = 0;
	r = f->ram;
	f->ram = 0;
	if(r->qid.path == 0 || !perm(f, &ram[r->parent], Pwrite))
		return Eperm;
	ram[r->parent].mtime = time(0);

	return realremove(r);
}

static  char *
rstat(Fid *f)
{
        DBGPRN(">%s() ", __FUNCTION__);

	if(f->ram->busy == 0)
		return Enotexist;
	rhdr.nstat = ramstat(f->ram, statbuf, sizeof statbuf);
	rhdr.stat = statbuf;

	return 0;
}


static  char *
rwstat(Fid *f)
{
	Ram *r, *s;
	Dir dir;
        DBGPRN(">%s() ", __FUNCTION__);

	if(f->ram->busy == 0)
		return Enotexist;
	convM2D(thdr.stat, thdr.nstat, &dir, (char*)statbuf);
	r = f->ram;

	/*
	 * To change length, must have write permission on file.
	 */
	if(dir.length!=~0 && dir.length!=r->ndata){
	 	if(!perm(f, r, Pwrite))
			return Eperm;
	}

	/*
	 * To change name, must have write permission in parent
	 * and name must be unique.
	 */
	if(dir.name[0]!='\0' && strcmp(dir.name, r->name)!=0){
	 	if(!perm(f, &ram[r->parent], Pwrite))
			return Eperm;
		for(s=ram; s<&ram[nram]; s++)
			if(s->busy && s->parent==r->parent)
			if(strcmp(dir.name, s->name)==0)
				return Eexist;
	}

	/*
	 * To change mode, must be owner or group leader.
	 * Because of lack of users file, leader=>group itself.
	 */
	if(dir.mode!=~0 && r->perm!=dir.mode){
		if(strcmp(f->user, r->user) != 0)
		if(strcmp(f->user, r->group) != 0)
			return Enotowner;
	}

	/*
	 * To change group, must be owner and member of new group,
	 * or leader of current group and leader of new group.
	 * Second case cannot happen, but we check anyway.
	 */
	if(dir.gid[0]!='\0' && strcmp(r->group, dir.gid)!=0){
		if(strcmp(f->user, r->user) == 0)
	//	if(strcmp(f->user, dir.gid) == 0)
			goto ok;
		if(strcmp(f->user, r->group) == 0)
		if(strcmp(f->user, dir.gid) == 0)
			goto ok;
		return Enotowner;
		ok:;
	}

	/* all ok; do it */
	if(dir.mode != ~0){
		dir.mode &= ~DMDIR;	/* cannot change dir bit */
		dir.mode |= r->perm&DMDIR;
		r->perm = dir.mode;
	}
	if(dir.name[0] != '\0'){
		free(r->name);
		r->name = estrdup(dir.name);
	}
	if(dir.gid[0] != '\0')
		r->group = estrdup(dir.gid);
	if(dir.length!=~0 && dir.length!=r->ndata){
		r->data = erealloc(r->data, dir.length);
		if(r->ndata < dir.length)
			memset(r->data+r->ndata, 0, dir.length-r->ndata);
		r->ndata = dir.length;
	}
	ram[r->parent].mtime = time(0);

	return 0;
}

static  uint
ramstat(Ram *r, uchar *buf, uint nbuf)
{
	int n;
	Dir dir;

        DBGPRN(">%s() ", __FUNCTION__);

	dir.name = r->name;
	dir.qid = r->qid;
	dir.mode = r->perm;
	dir.length = r->ndata;
	dir.uid = r->user;
	dir.gid = r->group;
	dir.muid = r->muid;
	dir.atime = r->atime;
	dir.mtime = r->mtime;
	n = convD2M(&dir, buf, nbuf);
	if(n > 2)
		return n;

	return 0;
}

static  Fid *
newfid(int fid)
{
	Fid *f, *ff;

        DBGPRN(">%s() ", __FUNCTION__);

	ff = 0;
	for(f = fids; f; f = f->next)
		if(f->fid == fid)
			return f;
		else if(!ff && !f->busy)
			ff = f;
	if(ff){
		ff->fid = fid;
		return ff;
	}
	f = emalloc(sizeof *f);
	f->ram = nil;
	f->fid = fid;
	f->next = fids;
	fids = f;

	return f;
}


//--------------------------------------------
static  void
io(void)
{
	char *err, buf[40];
	int n, pid, ctl;
	Fid *fid;

        DBGPRN(">%s() \n", __FUNCTION__);
	pid = getpid();

#if 0 //%---------------------------------------
	if(private){
		snprint(buf, sizeof buf, "/proc/%d/ctl", pid);
		ctl = open(buf, OWRITE);
		if(ctl < 0){
			fprint(2, "can't protect ramfs\n");
		}else{
			fprint(ctl, "noswap\n");
			fprint(ctl, "private\n");
			close(ctl);
		}
	}
#endif //%--------------------------------------

	for(;;){
		/*
		 * reading from a pipe or a network device
		 * will give an error after a few eof reads.
		 * however, we cannot tell the difference
		 * between a zero-length read and an interrupt
		 * on the processes writing to us,
		 * so we wait for the error.
		 */

		n = read9pmsg(mfd[0], mdata, messagesize);
		// DBGBRK("< read9msg( )->%d \n", n);

		if(n < 0){
#if 0 //%-------------------
			rerrstr(buf, sizeof buf);
			if(buf[0]=='\0' || strstr(buf, "hungup"))
				exits("");
#endif //%------------------
			error("mount read");
		}
		if(n == 0)
			continue;
		if(convM2S(mdata, n, &thdr) == 0)
			continue;

		if(debug){
		        fprint(2, "ramfs %d:<-%F\n", pid, &thdr);
		}

		if(thdr.type<0 || thdr.type>=(int)nelem(fcalls) || !fcalls[thdr.type])  //% ??
			err = "bad fcall type";
		else if(((fid=newfid(thdr.fid))==nil || !fid->ram) && needfid[thdr.type])
			err = "fid not in use";
		else
			err = (*fcalls[thdr.type])(fid);

		if(err){
			rhdr.type = Rerror;
			rhdr.ename = err;

			// l4printf_s("ramfs: %s\n", err);  //%%
		}else{
			rhdr.type = thdr.type + 1;
			rhdr.fid = thdr.fid;
		}
		rhdr.tag = thdr.tag;

		if(debug){
			fprint(2, "ramfs %d:->%F\n", pid, &rhdr);/**/
		}
		n = convS2M(&rhdr, mdata, messagesize);
		if(n == 0)
			error("convS2M error on write");

		L4_Yield();  //% ??????
		if(write(mfd[1], mdata, n) != n)
			error("mount write");
	}
}


static  int
perm(Fid *f, Ram *r, int p)
{
        DBGPRN(">%s() ", __FUNCTION__);

	if((p*Pother) & r->perm)
		return 1;
	if(strcmp(f->user, r->group)==0 && ((p*Pgroup) & r->perm))
		return 1;
	if(strcmp(f->user, r->user)==0 && ((p*Powner) & r->perm))
		return 1;

	return 0;
}

static  void
error(char *s)
{
	l4printf_s("Ramfs-err: %s \n", s);
	//	fprint(2, "%s: %s: %r\n", argv0, s);
	L4_Sleep(L4_Never); //%
	//	exits(s);
}

static  void *
emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(!p)
		error("out of memory");
	memset(p, 0, n);

	return p;
}

static  void *
erealloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(!p)
		error("out of memory");

	return p;
}

static  char *
estrdup(char *q)
{
	char *p;
	int n;

	n = strlen(q)+1;
	p = malloc(n);
	if(!p)
		error("out of memory");
	memmove(p, q, n);

	return p;
}

static  void
usage(void)
{
	fprint(2, "usage: %s [-Dips] [-m mountpoint] [-S srvname]\n", argv0);
	// exits("usage");
}

