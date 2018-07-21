//%
#include  <l4all.h>      //%
#include  <lp49/l_actobj.h> //%

#include <u.h>
#include <libc.h>
#include <draw.h>
//#include <thread.h>	// HK 20100131
#include <cursor.h>
#include <mouse.h>      //% #include <mouse.h>
#include <keyboard.h>   //% #include <keyboard.h>
#include <frame.h>
#include <fcall.h>

#include "dat.h"
#include "fns.h"

void _backtrace(); //%

static Image *scrtmp;

static
void
scrtemps(void)
{
	int h;

	if(scrtmp)
		return;
	h = BIG*Dy(screen->r);
	scrtmp = allocimage(display, Rect(0, 0, 32, h), screen->chan, 0, DWhite);
	if(scrtmp == nil)
		error("scrtemps");
}

void
freescrtemps(void)
{
	freeimage(scrtmp);
	scrtmp = nil;
}

static
Rectangle
scrpos(Rectangle r, uint p0, uint p1, uint tot)
{
	Rectangle q;
	int h;

	q = r;
	h = q.max.y-q.min.y;
	if(tot == 0)
		return q;
	if(tot > 1024*1024){
		tot>>=10;
		p0>>=10;
		p1>>=10;
	}
	if(p0 > 0)
		q.min.y += h*p0/tot;
	if(p1 < tot)
		q.max.y -= h*(tot-p1)/tot;
	if(q.max.y < q.min.y+2){
		if(q.min.y+2 <= r.max.y)
			q.max.y = q.min.y+2;
		else
			q.min.y = q.max.y-2;
	}
	return q;
}

void
wscrdraw(Window *w)
{
	Rectangle r, r1, r2;
	Image *b;

	scrtemps();
	if(w->i == nil){
print("\n wscrdraw:");_backtrace(); //%
		error("scrdraw");
	}
	r = w->scrollr;
	b = scrtmp;
	r1 = r;
	r1.min.x = 0;
	r1.max.x = Dx(r);
	r2 = scrpos(r1, w->org, w->org + w->_frame.nchars, w->nr); //%
	if(!eqrect(r2, w->lastsr)){
		w->lastsr = r2;
		/* move r1, r2 to (0,0) to avoid clipping */
		r2 = rectsubpt(r2, r1.min);
		r1 = rectsubpt(r1, r1.min);
		draw(b, r1, w->_frame.cols[BORD], nil, ZP);  //%
		draw(b, r2, w->_frame.cols[BACK], nil, ZP);  //%
		r2.min.x = r2.max.x-1;
		draw(b, r2, w->_frame.cols[BORD], nil, ZP); //%
		draw(w->i, r, b, nil, Pt(0, r1.min.y));
	}
}

void
wscrsleep(Window *w, uint dt)
{
	Timer	*timer;
	int y, b;
	//%	static Alt alts[3];

	timer = timerstart(dt);
	y = w->mc._mouse.xy.y;  //%
	b = w->mc._mouse.buttons;  //%
//% alts[0].c = timer->c; alts[0].v = nil; alts[0].op = CHANRCV;
//% alts[1].c = w->mc.c; alts[1].v = &w->mc._mouse; alts[1].op = CHANRCV;
//% alts[2].op = CHANEND;

	for(;;){
	        L_msgtag  msgtag;  //%
		msgtag = l_recv0(nil, INF, nil);
		
		switch(MLABEL(msgtag)){
		case 0:
			timerstop(timer);
			return;
		case 1:  // WMouse
		        if(abs(w->mc._mouse.xy.y-y)>2 || w->mc._mouse.buttons!=b){  //%
				timercancel(timer);
				return;
			}
			break;
		}
	}
}

void
wscroll(Window *w, int but)
{
	uint p0, oldp0;
	Rectangle s;
	int x, y, my, h, first;

	s = insetrect(w->scrollr, 1);
	h = s.max.y-s.min.y;
	x = (s.min.x+s.max.x)/2;
	oldp0 = ~0;
	first = TRUE;
	do{
		flushimage(display, 1);
		if(w->mc._mouse.xy.x<s.min.x || s.max.x<=w->mc._mouse.xy.x){  //%
			readmouse(&w->mc);
		}else{
		        my = w->mc._mouse.xy.y;  //%
			if(my < s.min.y)
				my = s.min.y;
			if(my >= s.max.y)
				my = s.max.y;
			if(!eqpt(w->mc._mouse.xy, Pt(x, my))){  //%
				wmovemouse(w, Pt(x, my));
				readmouse(&w->mc); /* absorb event generated by moveto() */
			}
			if(but == 2){
				y = my;
				if(y > s.max.y-2)
					y = s.max.y-2;
				if(w->nr > 1024*1024)
					p0 = ((w->nr>>10)*(y-s.min.y)/h)<<10;
				else
					p0 = w->nr*(y-s.min.y)/h;
				if(oldp0 != p0)
					wsetorigin(w, p0, FALSE);
				oldp0 = p0;
				readmouse(&w->mc);
				continue;
			}
			if(but == 1)
			  p0 = wbacknl(w, w->org, (my-s.min.y)/w->_frame.font->height);  //%
			else
			  p0 = w->org+frcharofpt(&w->_frame, Pt(s.max.x, my));  //% w
			if(oldp0 != p0)
				wsetorigin(w, p0, TRUE);
			oldp0 = p0;
			/* debounce */
			if(first){
				flushimage(display, 1);
				sleep(200);
				//%% nbrecv(w->mc.c, &w->mc._mouse);  //% Mouse
				first = FALSE;
			}
			wscrsleep(w, 100);
		}
	}while(w->mc._mouse.buttons & (1<<(but-1)));  //%
	while(w->mc._mouse.buttons)  //%
		readmouse(&w->mc);
}
