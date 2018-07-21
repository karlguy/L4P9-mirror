char *errmsg[ESIZE] = {
	[Enevermind]	"never mind",
	[Eformat]	"unknown format",
	[Eio]		"dossrv I/O error",
	[Enoauth]	"dossrv: authentication not required",
	[Enomem]	"server out of memory",
	[Enonexist]	"file not exist",   //%
	[Eperm]		"permission denied",
	[Enofilsys]	"no file system device specified",
	[Eauth]		"authentication failed",
	[Econtig]	"out of contiguous disk space",
	[Ebadfcall]	"bad fcall type",
	[Ebadstat]	"bad stat format",
	[Etoolong]	"file name too long",
	[Eversion]	"unknown 9P version",
	[Eerrstr]	"system call error",
};
