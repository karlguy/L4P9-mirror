//%%%%%%%%%%%%%
#include	<u.h>
#include	<libc.h>

int
getpid(void)
{
#if 1 //%---------------------
        int  pid;
	pid = _d("getpid", 0);
	return  pid;

#else //original
	char b[20];
	int f;

	memset(b, 0, sizeof(b));
	f = open("#c/pid", 0);
	if(f >= 0) {
		read(f, b, sizeof(b));
		close(f);
	}
	return atol(b);
#endif //---------------------
}
