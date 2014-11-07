#ifndef __WIMM_H__
#define __WIMM_H__

#define VERSION "2.2"

#ifdef __DOS__
#define PROGNAME "WIMM"
#endif

#ifdef __OS2__
#define PROGNAME "WIMM/2"
#endif

#ifdef __WIN32__
#define PROGNAME "WIMM/Win32"
#endif

#ifdef __UNIX__
#ifdef __FreeBSD__
#define PROGNAME "WIMM/FreeBSD"
#endif
#ifdef __CYGWIN__
#define PROGNAME "WIMM/Cygwin"
#endif
#ifdef __Linux__
#define PROGNAME "WIMM/Linux"
#endif
#ifdef __BEOS__
#define PROGNAME "WIMM/BeOS"
#endif
#ifndef PROGNAME
#define PROGNAME "WIMM/UNIX"
#endif
#endif

#ifndef PROGNAME
#error You must define __DOS__, __OS2__, __WIN32__ or __UNIX__ for your target!
#endif

#ifdef __UNIX__
#define strcmpi strcasecmp
#define strnicmp strncasecmp
#endif

struct my_idx
{
    dword fill;
    UMSGID umsgid;
    dword hash;
};

#define PASSTHRU  0x01
#define SCANNED   0x02
#define EXCLUDE   0x04
#define FORCED    0x08

typedef struct arearec
{
    char tag[60];               /* Official area tag                 */
    char dir[80];               /* Directory/Base name               */
    word base;                  /* MSGTYPE_SDM or MSGTYPE_SQUISH     */
    char status;                /* 0 = normal, 1 = passthru          */
    struct arearec *next;       /* Pointer to next area              */
}
AREA;

typedef struct _namelist
{
    char name[36];
    long hash;
    struct _namelist *next;
}
NAMELIST;

typedef struct _arealist
{
    char tag[60];
    struct _arealist *next;
}
AREALIST;

typedef struct _msglist
{
    UMSGID id;
    struct _msglist *next;
}
MSGLIST;

typedef struct _persmsglist
{
    char tag[60];
    char from[36];
    char subject[80];
    long number;
    struct _persmsglist *next;
}
PERSMSGLIST;

#define LIST      1
#define MOVE      2
#define COPY      3

#define LASTREAD  1
#define ALL       2

#define FGETS_BUFSIZE 4096

#endif
