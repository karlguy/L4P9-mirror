//%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#include	"u.h"
#include	"../port/lib.h"
#include	"../pc/mem.h"
#include	"../pc/dat.h"
#include	"../pc/fns.h"
#include	"../port/error.h"

#define	Image	IMAGE
#include	<draw.h>
#include	<memdraw.h>
#include	<cursor.h>
#include	"../pc/screen.h"
#include        "../errhandler-l4.h"

enum {
	ScrollUp = 0x08,
	ScrollDown = 0x10,
	ScrollLeft = 0x20,
	ScrollRight = 0x40,
};

typedef struct Mouseinfo	Mouseinfo;
typedef struct Mousestate	Mousestate;

struct Mousestate
{
	Point	xy;		/* mouse.xy */
	int	buttons;	/* mouse.buttons */
	ulong	counter;	/* increments every update */
	ulong	msec;		/* time of last event */
};

struct Mouseinfo
{
        Lock    _lock;  //%
        Mousestate  _mousestate;  //%
	int	dx;
	int	dy;
	int	track;		/* dx & dy updated */
	int	redraw;		/* update cursor on screen */
	ulong	lastcounter;	/* value when /dev/mouse read */
	ulong	lastresize;
	ulong	resize;
	Rendez	r;
        Ref     _ref;    //%
        QLock   _qlock;  //%
	int	open;
	int	inopen;
	int	acceleration;
	int	maxacc;
	Mousestate	queue[16];	/* circular buffer of click events */
	int	ri;		/* read index into queue */
	int	wi;		/* write index into queue */
	uchar	qfull;		/* queue is full */
};

enum
{
	CMbuttonmap,
	CMscrollswap,
	CMswap,
	CMwildcard,
};

static Cmdtab mousectlmsg[] =
{
       {CMbuttonmap,	"buttonmap",	0},
       {CMscrollswap,	"scrollswap",	0},
       {CMswap,		"swap",		1},
       {CMwildcard,	"*",		0},
};

Mouseinfo	mouse;
Cursorinfo	cursor;
int		mouseshifted;
int		kbdbuttons;
void		(*kbdmouse)(int);
Cursor		curs;

void	Cursortocursor(Cursor*);
int	mousechanged(void*);
static void mouseclock(void);
static void xkbdmouse(int);

enum{
	Qdir,
	Qcursor,
	Qmouse,
	Qmousein,
	Qmousectl,
};

static Dirtab mousedir[]={
	".",	{Qdir, 0, QTDIR},	0,			DMDIR|0555,
	"cursor",	{Qcursor},	0,			0666,
	"mouse",	{Qmouse},	0,			0666,
	"mousein",	{Qmousein},	0,			0220,
	"mousectl",	{Qmousectl},	0,			0220,
};

static uchar buttonmap[8] = {
	0, 1, 2, 3, 4, 5, 6, 7,
};
static int mouseswap;
static int scrollswap;
extern	Memimage*	gscreen;

static void
mousereset(void)
{
	if(!conf.monitor)
		return;

	curs = arrow;
	Cursortocursor(&arrow);
	/* redraw cursor about 30 times per second */
	addclock0link(mouseclock, 33);
}

static void
mousefromkbd(int buttons)
{
	kbdbuttons = buttons;
	mousetrack(0, 0, 0, TK2MS(MACHP(0)->ticks));
	//print("$"); // HK 20091031, test
}

static void
mouseinit(void)
{
	if(!conf.monitor)
		return;

	curs = arrow;
	Cursortocursor(&arrow);
	cursoron(1);
	kbdmouse = mousefromkbd;
}

static Chan*
mouseattach(char *spec)
{
	if(!conf.monitor)
	        ERROR_RETURN(Egreg, nil);  //%
	return devattach('m', spec);
}

static Walkqid*
mousewalk(Chan *c, Chan *nc, char **name, int nname)
{
	Walkqid *wq;

	wq = devwalk(c, nc, name, nname, mousedir, nelem(mousedir), devgen);
	if(wq != nil && wq->clone != c && wq->clone != nil && (wq->clone->qid.type&QTDIR)==0)
	        incref(&mouse._ref); //% _ref
	return wq;
}

static int
mousestat(Chan *c, uchar *db, int n)
{
	return devstat(c, db, n, mousedir, nelem(mousedir), devgen);
}

static Chan*
mouseopen(Chan *c, int omode) // ONERR
{
	switch((ulong)c->qid.path){
	case Qdir:
		if(omode != OREAD)
		  ERROR_RETURN(Eperm, nil);  //%
		break;
	case Qmouse:
	        lock(&mouse._lock);  //%
		if(mouse.open){
		        unlock(&mouse._lock);  //%
			ERROR_RETURN(Einuse, nil);
		}
		mouse.open = 1;
		mouse._ref.ref++; //%
		mouse.lastresize = mouse.resize;
		unlock(&mouse._lock);  //%
		break;
	case Qmousein:
		if(!iseve())
		        ERROR_RETURN(Eperm, nil);  //%
		lock(&mouse._lock);    //%
		if(mouse.inopen){
		        unlock(&mouse._lock);  //%
		        ERROR_RETURN(Einuse, nil);  //%
		}
		mouse.inopen = 1;
		unlock(&mouse._lock);  //%
		break;
	default:
	        incref(&mouse._ref);  //%
	}
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static int  //% void  ONERR
mousecreate(Chan* _w, char* _x, int _y, ulong _z)  //%
{
	if(!conf.monitor)
	        ERROR_RETURN(Egreg, ONERR); //%
	ERROR_RETURN(Eperm, ONERR);  //%
}

static void
mouseclose(Chan *c)
{
	if((c->qid.type&QTDIR)==0 && (c->flag&COPEN)){
	        lock(&mouse._lock);   //%
		if(c->qid.path == Qmouse)
			mouse.open = 0;
		else if(c->qid.path == Qmousein){
			mouse.inopen = 0;
			unlock(&mouse._lock);  //%
			return;
		}
		if(--mouse._ref.ref == 0){  //%
			cursoroff(1);
			curs = arrow;
			Cursortocursor(&arrow);
			cursoron(1);
		}
		unlock(&mouse._lock);  //%
	}
}


static long
mouseread(Chan *c, void *va, long n, vlong off)
{
	char buf[1+4*12+1];
	uchar *p;
	static int map[8] = {0, 4, 2, 6, 1, 5, 3, 7 };
	ulong offset = off;
	Mousestate m;
	int b;

	p = va;
	switch((ulong)c->qid.path){
	case Qdir:
		return devdirread(c, va, n, mousedir, nelem(mousedir), devgen);

	case Qcursor:
		if(offset != 0)
			return 0;
		if(n < 2*4+2*2*16)
		        ERROR_RETURN(Eshort, ONERR);  //%
		n = 2*4+2*2*16;
		lock(&cursor._lock);    //%
		BPLONG(p+0, curs.offset.x);
		BPLONG(p+4, curs.offset.y);
		memmove(p+8, curs.clr, 2*16);
		memmove(p+40, curs.set, 2*16);
		unlock(&cursor._lock);   //%
		return n;

	case Qmouse:
		if(n < 1+4*12)				// HK 20091231
		        ERROR_RETURN(Eshort, ONERR);	// HK 20091231

		while(mousechanged(0) == 0)
			sleep(&mouse.r, mousechanged, 0);

		mouse.qfull = 0;

		/*
		 * No lock of the indicies is necessary here, because ri is only
		 * updated by us, and there is only one mouse reader
		 * at a time.  I suppose that more than one process
		 * could try to read the fd at one time, but such behavior
		 * is degenerate and already violates the calling
		 * conventions for sleep above.
		 */
		if(mouse.ri != mouse.wi) {
			m = mouse.queue[mouse.ri];
			if(++mouse.ri == nelem(mouse.queue))
				mouse.ri = 0;
		} else {
		        while(!canlock(&cursor._lock))  //%
				tsleep(&up->sleep, return0, 0, TK2MS(1));
	
			m = mouse._mousestate;  //%
			unlock(&cursor._lock);  //%
		}

		b = buttonmap[m.buttons&7];
		/* put buttons 4 and 5 back in */
		b |= m.buttons & (3<<3);
		if (scrollswap)
			if (b == 8)
				b = 16;
			else if (b == 16)
				b = 8;
		sprint(buf, "m%11d %11d %11d %11lud ",
			m.xy.x, m.xy.y,
			b,
			m.msec);
		mouse.lastcounter = m.counter;
		if(n > 1+4*12)
			n = 1+4*12;
		if(mouse.lastresize != mouse.resize){
			mouse.lastresize = mouse.resize;
			buf[0] = 'r';
		}
		memmove(va, buf, n);
		return n;
	}
	return 0;
}

static int  //% void  
setbuttonmap(char* map)  // ONERR
{
	int i, x, one, two, three;

	one = two = three = 0;
	for(i = 0; i < 3; i++){
		if(map[i] == 0)
		  ERROR_RETURN(Ebadarg, ONERR);  //%
		if(map[i] == '1'){
			if(one)
			        ERROR_RETURN(Ebadarg, ONERR);  //%
			one = 1<<i;
		}
		else if(map[i] == '2'){
			if(two)
			        ERROR_RETURN(Ebadarg, ONERR);  //%
			two = 1<<i;
		}
		else if(map[i] == '3'){
			if(three)
			        ERROR_RETURN(Ebadarg, ONERR);  //%
			three = 1<<i;
		}
		else
		        ERROR_RETURN(Ebadarg, ONERR);  //%
	}
	if(map[i])
	        ERROR_RETURN(Ebadarg, ONERR);  //%

	memset(buttonmap, 0, 8);
	for(i = 0; i < 8; i++){
		x = 0;
		if(i & 1)
			x |= one;
		if(i & 2)
			x |= two;
		if(i & 4)
			x |= three;
		buttonmap[x] = i;
	}
}

static long
mousewrite(Chan *c, void *va, long n, vlong _x)  //% ONERR
{
	char *p;
	Point pt;
	Cmdbuf *cb;
	Cmdtab *ct;
	char buf[64];
	int b, msec;
  
	p = va;
	switch((ulong)c->qid.path){
	case Qdir:
	        ERROR_RETURN(Eisdir, ONERR);  //%

	case Qcursor:
		cursoroff(1);
		if(n < 2*4+2*2*16){
			curs = arrow;
			Cursortocursor(&arrow);
		}else{
			n = 2*4+2*2*16;
			curs.offset.x = BGLONG(p+0);
			curs.offset.y = BGLONG(p+4);
			memmove(curs.clr, p+8, 2*16);
			memmove(curs.set, p+40, 2*16);
			Cursortocursor(&curs);
		}
		qlock(&mouse._qlock);  //%
		mouse.redraw = 1;
		mouseclock();
		qunlock(&mouse._qlock);  //%
		cursoron(1);
		return n;

	case Qmousectl:
		cb = parsecmd(va, n);

		if(WASERROR()){  //%
		_ERR_1:
			free(cb);
			NEXTERROR_RETURN(ONERR);  //%
		}

		ct = lookupcmd(cb, mousectlmsg, nelem(mousectlmsg));

		switch(ct->index){
		case CMswap:
			if(mouseswap)
				setbuttonmap("123");
			else
				setbuttonmap("321");
			mouseswap ^= 1;
			break;

		case CMscrollswap:
			scrollswap ^= 1;
			break;

		case CMbuttonmap:
			if(cb->nf == 1)
				setbuttonmap("123");
			else
				setbuttonmap(cb->f[1]);
			break;

		case CMwildcard:
			mousectl(cb);
			break;
		}

		free(cb);
		POPERROR(); //%
		return n;

	case Qmousein:
		if(n > sizeof buf-1)
			n = sizeof buf -1;
		memmove(buf, va, n);
		buf[n] = 0;
		p = 0;
		pt.x = strtol(buf+1, &p, 0);
		if(p == 0)
		        ERROR_RETURN(Eshort, ONERR);  //%
		pt.y = strtol(p, &p, 0);
		if(p == 0)
		        ERROR_RETURN(Eshort, ONERR);  //%
		b = strtol(p, &p, 0);
		msec = strtol(p, &p, 0);
		if(msec == 0)
			msec = TK2MS(MACHP(0)->ticks);
		mousetrack(pt.x, pt.y, b, msec);
		return n;
		
	case Qmouse:
		if(n > sizeof buf-1)
			n = sizeof buf -1;
		memmove(buf, va, n);
		buf[n] = 0;
		p = 0;
		pt.x = strtoul(buf+1, &p, 0);
		if(p == 0)
		        ERROR_RETURN(Eshort, ONERR);  //%
		pt.y = strtoul(p, 0, 0);
		qlock(&mouse._qlock);  //%
		if(ptinrect(pt, gscreen->r)){
		  mouse._mousestate.xy = pt;  //%
			mouse.redraw = 1;
			mouse.track = 1;
			mouseclock();
		}
		qunlock(&mouse._qlock);  //%
		return n;
	}

	ERROR_RETURN(Egreg, ONERR);  //%
	return -1;
}

Dev mousedevtab = {
	'm',
	"mouse",

	mousereset,
	mouseinit,
	devshutdown,
	mouseattach,
	mousewalk,
	mousestat,
	mouseopen,
	mousecreate,  //%
	mouseclose,   //%
	mouseread,
	devbread,
	mousewrite,
	devbwrite,
	devremove,
	devwstat,
};

void
Cursortocursor(Cursor *c)
{
        lock(&cursor._lock);   //%
	memmove(&cursor._cursor, c, sizeof(Cursor));  //%
	setcursor(c);
	unlock(&cursor._lock);  //%
}


/*
 *  called by the clock routine to redraw the cursor
 */
static void
mouseclock(void)
{
	if(mouse.track){
	        mousetrack(mouse.dx, mouse.dy, mouse._mousestate.buttons, 
			   TK2MS(MACHP(0)->ticks));  //%
		mouse.track = 0;
		mouse.dx = 0;
		mouse.dy = 0;
	}
	if(mouse.redraw && canlock(&cursor._lock)){  //%
		mouse.redraw = 0;
		cursoroff(0);
		mouse.redraw = cursoron(0);
		unlock(&cursor._lock);  //%
	}
	drawactive(0);
}

static int
scale(int x)
{
	int sign = 1;

	if(x < 0){
		sign = -1;
		x = -x;
	}
	switch(x){
	case 0:
	case 1:
	case 2:
	case 3:
		break;
	case 4:
		x = 6 + (mouse.acceleration>>2);
		break;
	case 5:
		x = 9 + (mouse.acceleration>>1);
		break;
	default:
		x *= mouse.maxacc;
		break;
	}
	return sign*x;
}

/*
 *  called at interrupt level to update the structure and
 *  awaken any waiting procs.
 */
void
mousetrack(int dx, int dy, int b, int msec)
{
	int x, y, lastb;

	if(gscreen==nil)
		return;

	if(mouse.acceleration){
		dx = scale(dx);
		dy = scale(dy);
	}
	x = mouse._mousestate.xy.x + dx;  //%
	if(x < gscreen->clipr.min.x)
		x = gscreen->clipr.min.x;
	if(x >= gscreen->clipr.max.x)
		x = gscreen->clipr.max.x;
	y = mouse._mousestate.xy.y + dy;  //%
	if(y < gscreen->clipr.min.y)
		y = gscreen->clipr.min.y;
	if(y >= gscreen->clipr.max.y)
		y = gscreen->clipr.max.y;

	lastb = mouse._mousestate.buttons;  //%
	mouse._mousestate.xy = Pt(x, y);    //%
	mouse._mousestate.buttons = b|kbdbuttons;  //%
	mouse.redraw = 1;
	mouse._mousestate.counter++;  //%
	mouse._mousestate.msec = msec;  //%

	/*
	 * if the queue fills, we discard the entire queue and don't
	 * queue any more events until a reader polls the mouse.
	 */
	if(!mouse.qfull && lastb != b) {	/* add to ring */
	        mouse.queue[mouse.wi] = mouse._mousestate;  //%
		if(++mouse.wi == nelem(mouse.queue))
			mouse.wi = 0;
		if(mouse.wi == mouse.ri)
			mouse.qfull = 1;
	}
	wakeup(&mouse.r);
	drawactive(1);
}

/*
 *  microsoft 3 button, 7 bit bytes
 *
 *	byte 0 -	1  L  R Y7 Y6 X7 X6
 *	byte 1 -	0 X5 X4 X3 X2 X1 X0
 *	byte 2 -	0 Y5 Y4 Y3 Y2 Y1 Y0
 *	byte 3 -	0  M  x  x  x  x  x	(optional)
 *
 *  shift & right button is the same as middle button (for 2 button mice)
 */
int
m3mouseputc(Queue* _x, int c)
{
	static uchar msg[3];
	static int nb;
	static int middle;
	static uchar b[] = { 0, 4, 1, 5, 0, 2, 1, 3 };
	short x;
	int dx, dy, newbuttons;
	static ulong lasttick;
	ulong m;
	
	/* Resynchronize in stream with timing. */
	m = MACHP(0)->ticks;
	if(TK2SEC(m - lasttick) > 2)
		nb = 0;
	lasttick = m;

	if(nb==0){
		/*
		 * an extra byte comes for middle button motion.
		 * only two possible values for the extra byte.
		 */
		if(c == 0x00 || c == 0x20){
			/* an extra byte gets sent for the middle button */
			middle = (c&0x20) ? 2 : 0;
			newbuttons = (mouse._mousestate.buttons & ~2) | middle; //%
			mousetrack(0, 0, newbuttons, TK2MS(MACHP(0)->ticks));
			return 0;
		}
	}
	msg[nb] = c;
	if(++nb == 3){
		nb = 0;
		newbuttons = middle | b[(msg[0]>>4)&3 | (mouseshifted ? 4 : 0)];
		x = (msg[0]&0x3)<<14;
		dx = (x>>8) | msg[1];
		x = (msg[0]&0xc)<<12;
		dy = (x>>8) | msg[2];
		mousetrack(dx, dy, newbuttons, TK2MS(MACHP(0)->ticks));
	}
	return 0;
}

/*
 * microsoft intellimouse 3 buttons + scroll
 * 	byte 0 -	1  L  R Y7 Y6 X7 X6
 *	byte 1 -	0 X5 X4 X3 X2 X1 X0
 *	byte 2 -	0 Y5 Y4 Y3 Y2 Y1 Y0
 *	byte 3 -	0  0  M  %  %  %  %
 *
 *	%: 0xf => U , 0x1 => D
 *
 *	L: left
 *	R: right
 *	U: up
 *	D: down
 */
int
m5mouseputc(Queue* _x, int c)  //%
{
	static uchar msg[3];
	static int nb;
	static ulong lasttick;
	ulong m;
	
	/* Resynchronize in stream with timing. */
	m = MACHP(0)->ticks;
	if(TK2SEC(m - lasttick) > 2)
		nb = 0;
	lasttick = m;

	msg[nb++] = c & 0x7f;
	if (nb == 4) {
		schar dx,dy,newbuttons;
		dx = msg[1] | (msg[0] & 0x3) << 6;
		dy = msg[2] | (msg[0] & 0xc) << 4;
		newbuttons = 
			(msg[0] & 0x10) >> (mouseshifted ? 3 : 2)
			| (msg[0] & 0x20) >> 5
			| ( msg[3] == 0x10 ? 0x02 :
			    msg[3] == 0x0f ? ScrollUp :
			    msg[3] == 0x01 ? ScrollDown : 0 );
		mousetrack(dx, dy, newbuttons, TK2MS(MACHP(0)->ticks));
		nb = 0;
	}
	return 0;
}

/*
 *  Logitech 5 byte packed binary mouse format, 8 bit bytes
 *
 *  shift & right button is the same as middle button (for 2 button mice)
 */
int
mouseputc(Queue* _x, int c)
{
	static short msg[5];
	static int nb;
	static uchar b[] = {0, 4, 2, 6, 1, 5, 3, 7, 0, 2, 2, 6, 1, 3, 3, 7};
	int dx, dy, newbuttons;
	static ulong lasttick;
	ulong m;
	
	/* Resynchronize in stream with timing. */
	m = MACHP(0)->ticks;
	if(TK2SEC(m - lasttick) > 2)
		nb = 0;
	lasttick = m;

	if((c&0xF0) == 0x80)
		nb=0;
	msg[nb] = c;
	if(c & 0x80)
		msg[nb] |= ~0xFF;	/* sign extend */
	if(++nb == 5){
		newbuttons = b[((msg[0]&7)^7) | (mouseshifted ? 8 : 0)];
		dx = msg[1]+msg[3];
		dy = -(msg[2]+msg[4]);
		mousetrack(dx, dy, newbuttons, TK2MS(MACHP(0)->ticks));
		nb = 0;
	}
	return 0;
}

int
mousechanged(void* _x)
{
        return mouse.lastcounter != mouse._mousestate.counter ||   //%
		mouse.lastresize != mouse.resize;
}

Point
mousexy(void)
{
        return mouse._mousestate.xy;  //%
}

void
mouseaccelerate(int x)
{
	mouse.acceleration = x;
	if(mouse.acceleration < 3)
		mouse.maxacc = 2;
	else
		mouse.maxacc = mouse.acceleration;
}

/*
 * notify reader that screen has been resized
 */
void
mouseresize(void)
{
	mouse.resize++;
	wakeup(&mouse.r);
}
