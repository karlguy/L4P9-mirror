#include <u.h>
#include <libc.h>
#include <bio.h>

#include "pci.h"
#include "vga.h"

int vflag, Vflag;

void
error(char* format, ...)
{
	char buf[512], *out;
	va_list arg;
	int n;

	sequencer(0, 1);
	n = sprint(buf, "%s: ", argv0);
	va_start(arg, format);
	out = vseprint(buf+n, buf+sizeof(buf)-n, format, arg);
	va_end(arg);
	if(vflag)
		Bprint(&stdout._Biobufhdr, "%s", buf+n);	// HK 20090831
	Bflush(&stdout._Biobufhdr);	// HK 20090831
	write(2, buf, out-buf);
	exits("error");
}

void
trace(char* format, ...)
{
	char buf[512];
	va_list arg;

	if(vflag || Vflag){
		if(curprintindex){
			curprintindex = 0;
			Bprint(&stdout._Biobufhdr, "\n");	// HK 20090831
		}
		va_start(arg, format);
		vseprint(buf, buf+sizeof(buf), format, arg);
		va_end(arg);
		Bprint(&stdout._Biobufhdr, "%s", buf);	// HK 20090831
		if(Vflag)
			print("%s", buf);
	}
}

