#include <u.h>
#include <libc.h>
#include <bio.h>

#include "pci.h"
#include "vga.h"

enum {
	NSeqx		= 0x05,
	NCrtx		= 0x19,
	NGrx		= 0x09,
	NAttrx		= 0x15,
};

uchar
vgai(long port)
{
	uchar data;

	switch(port){

	case MiscR:
	case Status0:
	case Status1:
	case FeatureR:
	case PaddrW:
	case Pdata:
	case Pixmask:
	case Pstatus:
		data = inportb(port);
		break;

	default:
		error("vgai(0x%4.4lX): unknown port\n", port);
		/*NOTREACHED*/
		data = 0xFF;
		break;
	}
	return data;
}

uchar
vgaxi(long port, uchar index)
{
	uchar data;

	switch(port){

	case Seqx:
	case Crtx:
	case Grx:
		outportb(port, index);
		data = inportb(port+1);
		break;

	case Attrx:
		/*
		 * Allow processor access to the colour
		 * palette registers. Writes to Attrx must
		 * be preceded by a read from Status1 to
		 * initialise the register to point to the
		 * index register and not the data register.
		 * Processor access is allowed by turning
		 * off bit 0x20.
		 */
		inportb(Status1);
		if(index < 0x10){
			outportb(Attrx, index);
			data = inportb(Attrx+1);
			inportb(Status1);
			outportb(Attrx, 0x20|index);
		}
		else{
			outportb(Attrx, 0x20|index);
			data = inportb(Attrx+1);
		}
		break;

	default:
		error("vgaxi(0x%4.4lx, 0x%2.2uX): unknown port\n", port, index);
		/*NOTREACHED*/
		data = 0xFF;
		break;
	}
	return data;
}

void
vgao(long port, uchar data)
{
	switch(port){

	case MiscW:
	case FeatureW:
	case PaddrW:
	case Pdata:
	case Pixmask:
	case PaddrR:
		outportb(port, data);
		break;

	default:
		error("vgao(0x%4.4lX, 0x%2.2uX): unknown port\n", port, data);
		/*NOTREACHED*/
		break;
	}
}

void
vgaxo(long port, uchar index, uchar data)
{
	switch(port){

	case Seqx:
	case Crtx:
	case Grx:
		/*
		 * We could use an outport here, but some chips
		 * (e.g. 86C928) have trouble with that for some
		 * registers.
		 */
		outportb(port, index);
		outportb(port+1, data);
		break;

	case Attrx:
		inportb(Status1);
		if(index < 0x10){
			outportb(Attrx, index);
			outportb(Attrx, data);
			inportb(Status1);
			outportb(Attrx, 0x20|index);
		}
		else{
			outportb(Attrx, 0x20|index);
			outportb(Attrx, data);
		}
		break;

	default:
		error("vgaxo(0x%4.4lX, 0x%2.2uX, 0x%2.2uX): unknown port\n",
			port, index, data);
		break;
	}
}

static void
snarf(Vga* vga, Ctlr* ctlr)
{
	int i;

	/*
	 * Generic VGA registers:
	 * 	misc, feature;
	 *	sequencer;
	 *	crt;
	 *	graphics;
	 *	attribute;
	 *	palette.
	 */
	vga->misc = vgai(MiscR);
	vga->feature = vgai(FeatureR);
	vga->apz = 64 * 1024;	// HK 20090930

	for(i = 0; i < NSeqx; i++)
		vga->sequencer[i] = vgaxi(Seqx, i);

	for(i = 0; i < NCrtx; i++)
		vga->crt[i] = vgaxi(Crtx, i);

	for(i = 0; i < NGrx; i++)
		vga->graphics[i] = vgaxi(Grx, i);

	for(i = 0; i < NAttrx; i++)
		vga->attribute[i] = vgaxi(Attrx, i);

	if(dflag)
		palette.snarf(vga, ctlr);

	ctlr->flag |= Fsnarf;
}

static void
init(Vga* vga, Ctlr* ctlr)
{
	Mode *mode;
	int vt, vde, vrs, vre;
	ulong tmp;

	mode = vga->mode;

	memset(vga->sequencer, 0, NSeqx*sizeof(vga->sequencer[0]));
	memset(vga->crt, 0, NCrtx*sizeof(vga->crt[0]));
	memset(vga->graphics, 0, NGrx*sizeof(vga->graphics[0]));
	memset(vga->attribute, 0, NAttrx*sizeof(vga->attribute[0]));
	if(dflag)
		memset(vga->palette, 0, sizeof(vga->palette));

	/*
	 * Misc. If both the horizontal and vertical sync polarity
	 * options are set, use them. Otherwise use the defaults for
	 * the given vertical size.
	 */
	vga->misc = 0x23;
	if(mode->frequency == VgaFreq1)
		vga->misc |= 0x04;
	if(mode->hsync && mode->vsync){
		if(mode->hsync == '-')
			vga->misc |= 0x40;
		if(mode->vsync == '-')
			vga->misc |= 0x80;
	}
	else{
		if(mode->y < 480)
			vga->misc |= 0x40;
		else if(mode->y < 400)
			vga->misc |= 0x80;
		else if(mode->y < 768)
			vga->misc |= 0xC0;
	}

	/*
	 * Sequencer
	 */
	vga->sequencer[0x00] = 0x03;
	vga->sequencer[0x01] = 0x01;
	vga->sequencer[0x02] = 0x0F;
	vga->sequencer[0x03] = 0x00;
	if(mode->z >= 8)
		vga->sequencer[0x04] = 0x0A;
	else
		vga->sequencer[0x04] = 0x06;

	/*
	 * Crt. Most of the work here is in dealing
	 * with field overflow.
	 */
	memset(vga->crt, 0, NCrtx);

	vga->crt[0x00] = (mode->ht>>3)-5;
	vga->crt[0x01] = (mode->x>>3)-1;
	vga->crt[0x02] = (mode->shb>>3)-1;

	/*
	 * End Horizontal Blank is a 6-bit field, 5-bits
	 * in Crt3, high bit in Crt5.
	 */
	tmp = mode->ehb>>3;
	vga->crt[0x03] = 0x80|(tmp & 0x1F);
	if(tmp & 0x20)
		vga->crt[0x05] |= 0x80;

	if(mode->shs == 0)
		mode->shs = mode->shb;
	vga->crt[0x04] = mode->shs>>3;
	if(mode->ehs == 0)
		mode->ehs = mode->ehb;
	vga->crt[0x05] |= ((mode->ehs>>3) & 0x1F);

	/*
	 * Vertical Total is 10-bits, 8 in Crt6, the high
	 * two bits in Crt7. What if vt is >= 1024? We hope
	 * the specific controller has some more overflow
	 * bits.
	 *
	 * Interlace: if 'v',  divide the vertical timing
	 * values by 2.
	 */
	vt = mode->vt;
	vde = mode->y;
	vrs = mode->vrs;
	vre = mode->vre;

	if(mode->interlace == 'v'){
		vt /= 2;
		vde /= 2;
		vrs /= 2;
		vre /= 2;
	}

	tmp = vt-2;
	vga->crt[0x06] = tmp;
	if(tmp & 0x100)
		vga->crt[0x07] |= 0x01;
	if(tmp & 0x200)
		vga->crt[0x07] |= 0x20;

	tmp = vrs;
	vga->crt[0x10] = tmp;
	if(tmp & 0x100)
		vga->crt[0x07] |= 0x04;
	if(tmp & 0x200)
		vga->crt[0x07] |= 0x80;

	vga->crt[0x11] = 0x20|(vre & 0x0F);

	tmp = vde-1;
	vga->crt[0x12] = tmp;
	if(tmp & 0x100)
		vga->crt[0x07] |= 0x02;
	if(tmp & 0x200)
		vga->crt[0x07] |= 0x40;

	vga->crt[0x15] = vrs;
	if(vrs & 0x100)
		vga->crt[0x07] |= 0x08;
	if(vrs & 0x200)
		vga->crt[0x09] |= 0x20;

	vga->crt[0x16] = (vrs+1);

	vga->crt[0x17] = 0x83;
	tmp = ((vga->virtx*mode->z)/8);
	if(tmp >= 512){
		vga->crt[0x14] |= 0x60;
		tmp /= 8;
	}
	else if(tmp >= 256){
		vga->crt[0x17] |= 0x08;
		tmp /= 4;
	}
	else{
		vga->crt[0x17] |= 0x40;
		tmp /= 2;
	}
	vga->crt[0x13] = tmp;

	if(mode->x*mode->y*mode->z/8 > 64*1024)
		vga->crt[0x17] |= 0x20;

	vga->crt[0x18] = 0x7FF;
	if(vga->crt[0x18] & 0x100)
		vga->crt[0x07] |= 0x10;
	if(vga->crt[0x18] & 0x200)
		vga->crt[0x09] |= 0x40;

	/*
	 * Graphics
	 */
	memset(vga->graphics, 0, NGrx);
	if((vga->sequencer[0x04] & 0x04) == 0)
		vga->graphics[0x05] |= 0x10;
	if(mode->z >= 8)
		vga->graphics[0x05] |= 0x40;
	vga->graphics[0x06] = 0x05;
	vga->graphics[0x07] = 0x0F;
	vga->graphics[0x08] = 0xFF;

	/*
	 * Attribute
	 */
	memset(vga->attribute, 0, NAttrx);
	for(tmp = 0; tmp < 0x10; tmp++)
		vga->attribute[tmp] = tmp;
	vga->attribute[0x10] = 0x01;
	if(mode->z >= 8)
		vga->attribute[0x10] |= 0x40;
	vga->attribute[0x11] = 0xFF;
	vga->attribute[0x12] = 0x0F;

	/*
	 * Palette
	 */
	if(dflag)
		palette.init(vga, ctlr);

	ctlr->flag |= Finit;
}

#if 0

static void
load(Vga* vga, Ctlr* ctlr)
{
	int i;

	/*
	 * Reset the sequencer and leave it off.
	 * Load the generic VGA registers:
	 *	misc;
	 *	sequencer (but not seq01, display enable);
	 *	take the sequencer out of reset;
	 *	take off write-protect on crt[0x00-0x07];
	 *	crt;
	 *	graphics;
	 *	attribute;
	 *	palette.
	vgaxo(Seqx, 0x00, 0x00);
	 */

	// HK 20090930 begin

	if (vga->virtx == 320 && vga->virty == 200) {
		static unsigned char seq[] = {
			0x03, 0x01, 0x0f, 0x00, 0x0e
		};
		static unsigned char crt[] = {
			0x5f, 0x4f, 0x50, 0x82, 0x55, 0x81, 0xbf, 0x1f,
			0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x9c, 0x8e, 0x8f, 0x28, 0x40, 0x96, 0xb9, 0xa3,
			0xff
		};
		static unsigned char grp[] = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0f, 0xff
		};
		static unsigned char atr[] = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
			0x10, 0x00, 0x12, 0x00, 0x04
		};

		vgaxo(Seqx, 0x00, 0x01);
		outportb(0x03c2, 0xe3);
		outportb(0x03c3, 0x01);

		for (i = 0x01; i <= 0x04; i++)
			vgaxo(Seqx, i, seq[i]);
		vgaxo(Seqx, 0x00, seq[0]);

		vgaxo(Crtx, 0x11, 0x20);
		for (i = 0x00; i <= 0x18; i++)
			vgaxo(Crtx, i, crt[i]);

		for (i = 0x00; i <= 0x08; i++)
			vgaxo(Grx, i, grp[i]);

		for (i = 0x00; i <= 0x14; i++)
			vgaxo(Attrx, i, atr[i]);

		if(dflag)
			palette.load(vga, ctlr);
		ctlr->flag |= Fload;
		return;
	}

	if (vga->virtx == 640 && vga->virty == 480) {
		static unsigned char seq[] = {
			0x03, 0x01, 0x0f, 0x00, 0x06
		};
		static unsigned char crt[] = {
			0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0x0b, 0x3e,
			0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xea, 0x8c, 0xdf, 0x28, 0x00, 0xe7, 0x04, 0xe3,
			0xff
		};
		static unsigned char grp[] = {
			0x00, 0x0f, 0x00, 0x00, 0x00, 0x03, 0x05, 0x00, 0xff
		};
		static unsigned char atr[] = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
			0x01, 0x00, 0x0f, 0x00, 0x00
		};

		vgaxo(Seqx, 0x00, 0x01);
		outportb(0x03c2, 0xe3);
		outportb(0x03c3, 0x01);

		for (i = 0x01; i <= 0x04; i++)
			vgaxo(Seqx, i, seq[i]);
		vgaxo(Seqx, 0x00, seq[0]);

		vgaxo(Crtx, 0x11, 0x20);
		for (i = 0x00; i <= 0x18; i++)
			vgaxo(Crtx, i, crt[i]);

		for (i = 0x00; i <= 0x08; i++)
			vgaxo(Grx, i, grp[i]);

		for (i = 0x00; i <= 0x14; i++)
			vgaxo(Attrx, i, atr[i]);

		if(dflag)
			palette.load(vga, ctlr);
		ctlr->flag |= Fload;
		return;
	}

#if 0

	vgao(MiscW, vga->misc);

	for(i = 2; i < NSeqx; i++)
		vgaxo(Seqx, i, vga->sequencer[i]);
	/*vgaxo(Seqx, 0x00, 0x03);*/

	vgaxo(Crtx, 0x11, vga->crt[0x11] & ~0x80);
	for(i = 0; i < NCrtx; i++)
		vgaxo(Crtx, i, vga->crt[i]);

	for(i = 0; i < NGrx; i++)
		vgaxo(Grx, i, vga->graphics[i]);

	for(i = 0; i < NAttrx; i++)
		vgaxo(Attrx, i, vga->attribute[i]);

	if(dflag)
		palette.load(vga, ctlr);

#endif

	// HK 20090930 end

	ctlr->flag |= Fload;
}

#endif

static void
dump(Vga* vga, Ctlr* ctlr)
{
	int i;

	printitem(ctlr->name, "misc");
	printreg(vga->misc);
	printitem(ctlr->name, "feature");
	printreg(vga->feature);

	printitem(ctlr->name, "sequencer");
	for(i = 0; i < NSeqx; i++)
		printreg(vga->sequencer[i]);

	printitem(ctlr->name, "crt");
	for(i = 0; i < NCrtx; i++)
		printreg(vga->crt[i]);

	printitem(ctlr->name, "graphics");
	for(i = 0; i < NGrx; i++)
		printreg(vga->graphics[i]);

	printitem(ctlr->name, "attribute");
	for(i = 0; i < NAttrx; i++)
		printreg(vga->attribute[i]);

	if(dflag)
		palette.dump(vga, ctlr);

	printitem(ctlr->name, "virtual");
	Bprint(&stdout._Biobufhdr, "%ld %ld\n", vga->virtx, vga->virty);	// HK 20090831
	printitem(ctlr->name, "panning");
	Bprint(&stdout._Biobufhdr, "%s\n", vga->panning ? "on" : "off");	// HK 20090831
	if(vga->f[0]){
		printitem(ctlr->name, "clock[0] f");
		Bprint(&stdout._Biobufhdr, "%9ld\n", vga->f[0]);		// HK 20090831
		printitem(ctlr->name, "clock[0] d i m");
		Bprint(&stdout._Biobufhdr, "%9ld %8ld       - %8ld\n",		// HK 20090831 
			vga->d[0], vga->i[0], vga->m[0]);
		printitem(ctlr->name, "clock[0] n p q r");
		Bprint(&stdout._Biobufhdr, "%9ld %8ld       - %8ld %8ld\n",	// HK 20090831
			vga->n[0], vga->p[0], vga->q[0], vga->r[0]);
	}
	if(vga->f[1]){
		printitem(ctlr->name, "clock[1] f");
		Bprint(&stdout._Biobufhdr, "%9ld\n", vga->f[1]);	// HK 20090831
		printitem(ctlr->name, "clock[1] d i m");
		Bprint(&stdout._Biobufhdr, "%9ld %8ld       - %8ld\n",	// HK 20090831
			vga->d[1], vga->i[1], vga->m[1]);
		printitem(ctlr->name, "clock[1] n p q r");
		Bprint(&stdout._Biobufhdr, "%9ld %8ld       - %8ld %8ld\n",	// HK 20090831
			vga->n[1], vga->p[1], vga->q[1], vga->r[1]);
	}

	if(vga->vma || vga->vmb){
		printitem(ctlr->name, "vm a b");
		Bprint(&stdout._Biobufhdr, "%9lud %8lud\n", vga->vma, vga->vmb);	// HK 20090831
	}
	if(vga->vmz){
		printitem(ctlr->name, "vmz");
		Bprint(&stdout._Biobufhdr, "%9lud\n", vga->vmz);		// HK 20090831
	}
	printitem(ctlr->name, "apz");
	Bprint(&stdout._Biobufhdr, "%9lud\n", vga->apz);		// HK 20090831

	printitem(ctlr->name, "linear");
	Bprint(&stdout._Biobufhdr, "%9d\n", vga->linear);	// HK 20090831

	// HK 20090930 begin
	printitem("vga", "mode");
	Bprint(&stdout._Biobufhdr, "0x12 640x480x8 vga-plane (640x480x4)\n");
	printitem("vga", "mode");
	Bprint(&stdout._Biobufhdr, "0x13 320x200x8 packed\n");
//	printitem("vga", "mode");
//	Bprint(&stdout._Biobufhdr, "0x6a 800x600x8 vga-plane (800x600x4)\n");
	// HK 20090930 end

	return;
}

// HK 20090930 begin

static char *probe(Vga *vga, Ctlr *ctlr)
{
	return "0xC0000=ctlr:vga";
}

#if 0

static void loadtext(Vga *vga, Ctlr *ctlr)
{
#if 0
	if (strcmp(vga->ctlr->name, "vga") == 0) {
		Bprint(&stdout._Biobufhdr, "vga(generic): loadtext() is not supported.\n");
		Bflush(&stdout._Biobufhdr);
		exits(0);
	}
#endif
	return;
}

#endif

static Mode *dbmode_(char *monitordb, char *type, char *mode)
{
	int x = 0, y, z, id;
	Mode *m = nil;

	if (strcmp(mode, "640x480x8") == 0) {
		x = 640;
		y = 480;
		z = 8;
		id = 0x12;
	}
	if (strcmp(mode, "320x200x8") == 0) {
		x = 320;
		y = 200;
		z = 8;
		id = 0x13;
	}
	if (strcmp(mode, "800x600x8") == 0) {
		x = 800;
		y = 600;
		z = 8;
		id = 0x6a;
	}
	if (x > 0) {
		m = alloc(sizeof(Mode));
		strcpy(m->type, "vga");
		strcpy(m->size, mode);
		strcpy(m->chan, "m8");
		m->frequency = 100;
		m->x = x;
		m->y = y;
		m->z = z;
		m->ht = x;
		m->shb = x;
		m->ehb = x;
		m->shs = x;
		m->ehs = x;
		m->vt = y;
		m->vrs = y;
		m->vre = y;
		m->attr = alloc(sizeof(Attr));
		m->attr->attr = "id";
		m->attr->val = alloc(32);
		sprint(m->attr->val, "0x%x", 0 /* id, 0x12 etc. */);
	}
	return m;
}

// HK 20090930 end

Ctlr generic = {
	"vga",				/* name */
	snarf,				/* snarf */
	0,				/* options */
	init,				/* init */
	0,				/* load */
	dump,				/* dump */
	0,				/* loadtext */	// HK 20090930
	probe,				/* probe */		// HK 20090930
	dbmode_,			/* dbmode */		// HK 20090930
	Avesalinear | Ulinear,	/* flag */		// HK 20090930
};

