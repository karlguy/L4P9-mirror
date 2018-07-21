#include	<u.h>
#include	<libc.h>
#include	<fcall.h>

/* input: Dir
 * output:  see bellow.
 *
 * buf
 *  |
 *  |<---------------------- nbuf ---------------------------------->|
 *  +---+-------------+---+----+---+----+---+----+---+-----+---------+
 *  |sz0| dir-fixed   |sz1|name|sz2|uid.|sz3|gid.|sz4|muid.|-----    |
 *  +---+-------------+---+----+---+----+---+----+---+-----+---------+
 *      |<---------------- sz0 --------------------------->|
 *  |<--------- return val ------------------------------->|
 *
 *
 * STATFIXLEN = SZ(2) + sizeof(Dir) -4*4 + 4*2   
 */


uint
sizeD2M(Dir *d)
{
	char *sv[4];
	int i, ns;

	sv[0] = d->name;
	sv[1] = d->uid;
	sv[2] = d->gid;
	sv[3] = d->muid;

	ns = 0;
	for(i = 0; i < 4; i++)
		if(sv[i])
			ns += strlen(sv[i]);

	return STATFIXLEN + ns;
}

uint
convD2M(Dir *d, uchar *buf, uint nbuf)
{
	uchar *p, *ebuf;
	char *sv[4];
	int i, ns, nsv[4], ss;

	if(nbuf < BIT16SZ)
		return 0;

	p = buf;
	ebuf = buf + nbuf;

	sv[0] = d->name;
	sv[1] = d->uid;
	sv[2] = d->gid;
	sv[3] = d->muid;

	ns = 0;
	for(i = 0; i < 4; i++){
		if(sv[i])
			nsv[i] = strlen(sv[i]);
		else
			nsv[i] = 0;
		ns += nsv[i];
	}

	ss = STATFIXLEN + ns;

	/* set size before erroring, so user can know how much is needed */
	/* note that length excludes count field itself */
	PBIT16(p, ss-BIT16SZ);
	p += BIT16SZ;

	if(ss > nbuf)
		return BIT16SZ;

	PBIT16(p, d->type);
	p += BIT16SZ;
	PBIT32(p, d->dev);
	p += BIT32SZ;
	PBIT8(p, d->qid.type);
	p += BIT8SZ;
	PBIT32(p, d->qid.vers);
	p += BIT32SZ;
	PBIT64(p, d->qid.path);
	p += BIT64SZ;
	PBIT32(p, d->mode);
	p += BIT32SZ;
	PBIT32(p, d->atime);
	p += BIT32SZ;
	PBIT32(p, d->mtime);
	p += BIT32SZ;
	PBIT64(p, d->length);
	p += BIT64SZ;

	for(i = 0; i < 4; i++){
		ns = nsv[i];
		if(p + ns + BIT16SZ > ebuf)
			return 0;
		PBIT16(p, ns);
		p += BIT16SZ;
		if(ns)
			memmove(p, sv[i], ns);
		p += ns;
	}

	if(ss != p - buf)
		return 0;

	return p - buf;
}