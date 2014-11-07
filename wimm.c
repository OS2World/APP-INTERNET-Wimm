/*
 *  WIMM; Where Is My Mail; Personal mail scanner for Squish & *.MSG
 *
 *  See LICENCE for copyright details.
 *
 *  wimm.c  Main module
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifndef __UNIX__
#include <share.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <sys/stat.h>

#if defined(__WATCOMC__) || defined(__BORLANDC__) || defined(_MSC_VER)
#include <io.h>
#include <fcntl.h>
#endif

#include "msgapi.h"

#include "wimm.h"
#include "wimmcfg.h"

#ifndef MSGTYPE_LOCAL
#define MSGTYPE_LOCAL 0x0100
#endif

static PERSMSGLIST *firstpers = NULL;

static char cfgname[200] = "wimm.cfg";
static char TossLogFile[200] = "";
static char currentarea[80] = "";

typedef struct idxdata
{
    struct my_idx *idxptr;
    long n;
    long last;
}
IDXDATA;

static IDXDATA idxmem;
static int logfd, dolog, foundmail;

char *getline(char *dest, int n, FILE *fp);

static char *mtext[] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *stristr(char *s1, char *s2)
{
    char *pptr, *sptr, *start;
    size_t slen, plen;

    start = s1;
    slen = strlen(s1);
    plen = strlen(s2);

    /* while string length not shorter than pattern length */

    while (slen >= plen)
    {
        /* find start of pattern in string */

        while (toupper(*start) != toupper(*s2))
        {
            start++;
            slen--;

            /* if pattern longer than string */

            if (slen < plen)
            {
                return NULL;
            }
        }

        sptr = start;
        pptr = s2;

        while (toupper(*sptr) == toupper(*pptr))
        {
            sptr++;
            pptr++;

            /* if end of pattern then pattern was found */

            if (*pptr == '\0')
            {
                return start;
            }
        }

        start++;
        slen--;
    }

    return NULL;
}

/* Write a line to the logfile (if logging is on..) */

static void logit(char *line, int hrt)
{
    time_t ltime;
    struct tm *tp;
    char temp[200];

    if (!dolog)
    {
        return;
    }

    time(&ltime);

    tp = localtime(&ltime);

    if (strlen(line) > 50)
    {
        line[50] = '\0';
    }

    if (hrt)
    {
        sprintf(temp, "\n  %02i %3s %02i:%02i:%02i WIMM %s",
          tp->tm_mday, mtext[tp->tm_mon], tp->tm_hour, tp->tm_min, tp->tm_sec,
          line);
    }
    else
    {
        sprintf(temp, "%s", line);
    }

    write(logfd, temp, strlen(temp));
}

static void AddMessage(char *tag, long number, char *from, char *subject)
{
    PERSMSGLIST *thismsg;
    static PERSMSGLIST *lastmsg;

    thismsg = malloc(sizeof(PERSMSGLIST));

    if (firstpers == NULL)
    {
        firstpers = thismsg;
    }
    else
    {
        lastmsg->next = thismsg;
    }

    strcpy(thismsg->tag, tag);
    strcpy(thismsg->from, from);
    strcpy(thismsg->subject, subject);

    thismsg->number = number;
    thismsg->next = NULL;

    lastmsg = thismsg;
}

static void ProcessMessages(MSGA * areahandle, MSGLIST * firstmsg, AREA * curarea)
{
    char *ctxt, *msgtxt, temp[256], msg[80], errortext[200];
    dword ctrllen, totlen, origlen, safe, msgno;
    MSGA *toarea;
    XMSG *header;
    MSGH *msghandle;
    MSGLIST *thismsg;
    NAMELIST *curname;

    foundmail = 1;

    sprintf(msg, "Personal mail found in %s!", currentarea);
    logit(msg, 1);

    toarea = MsgOpenArea(LocalArea, MSGAREA_CRIFNEC, (word) (LocalType | MSGTYPE_LOCAL));

    if (toarea == NULL)
    {
        fprintf(stderr, "Error opening local area (%s)!\n", LocalArea);
    }

    if (MsgLock(toarea) == -1)
    {
        fprintf(stderr, "Error locking local area (%s)!\n", LocalArea);
    }

    thismsg = firstmsg;

    while (thismsg)
    {
        /* Used to double-check if a message is TO: you..! */

        safe = 0;               

        msgno = MsgUidToMsgn(areahandle, thismsg->id, UID_EXACT);
        
        if (msgno == 0)
        {
            thismsg = thismsg->next;
            fprintf(stderr, "UID #%d in %s doesn't exist..\n", (int) thismsg->id, curarea->tag);
            continue;
        }

        msghandle = MsgOpenMsg(areahandle, MOPEN_RW, msgno);
        
        if (msghandle == NULL)
        {
            fprintf(stderr, "Error opening message #%d!\n", (int) msgno);
            thismsg = thismsg->next;
            continue;
        }

        ctrllen = MsgGetCtrlLen(msghandle);
        totlen = MsgGetTextLen(msghandle);

        header = calloc(1, sizeof(XMSG));

        if (ctrllen)
        {
            /* Add 100 for possible AREA: kludge */
            ctxt = calloc(1, ctrllen + 100);  
        }
        else
        {
            ctxt = NULL;
        }

        if (totlen)
        {
            msgtxt = calloc(1, totlen);
        }
        else
        {
            msgtxt = NULL;
        }

        if (MsgReadMsg(msghandle, header, 0L, totlen, msgtxt, ctrllen, ctxt) == (dword) -1)
        {
            fprintf(stderr, "Error reading message #%d!\n", (int) msgno);
            MsgCloseMsg(msghandle);
            goto close_all;
        }

        if (header->attr & MSGREAD)
        {
            MsgCloseMsg(msghandle);
            printf("Msg from %s (rcvd, skipping..)\n", header->from);
            goto close_all;
        }

        if (markreceived)
        {
            if (MsgLock(areahandle) == -1)
            {
                fprintf(stderr, "Error locking original area\n");
            }

            header->attr |= MSGREAD;

            if (MsgWriteMsg(msghandle, 0, header, 0L, 0L, 0L, 0L, 0L) == -1)
            {
                fprintf(stderr, "Error setting 'received' status!\n");
            }

            if (MsgUnlock(areahandle) == -1)
            {
                fprintf(stderr, "Error unlocking original area\n");
            }
        }

        if (MsgCloseMsg(msghandle) == -1)
        {
            fprintf(stderr, "Error closing msg #%d\n", (int) msgno);
        }

        /* Double check if it's a personal msg */

        curname = firstname;

        while (curname)
        {
            /* Try all names in list */

            if (strcmpi(header->to, curname->name) == 0)
            {
                /* Yep! Match... */
                safe = 1;
                break;
            }
            curname = curname->next;  /* Select next "alias" */
        }

        if (!safe)
        {
            fprintf(stderr, "Message %d doesn't seem to be personal anyway?!\n", (int)msgno);
            goto close_all;
        }

        printf("Msg from %-26s (%-39s)\n", header->from, header->subj);

        if (mode == MOVE || mode == COPY)
        {
            if (mode == MOVE)
            {
                if (MsgKillMsg(areahandle, msgno) == -1)
                {
                    fprintf(stderr, "Cannot kill message #%d!\n", (int) msgno);
                }
            }

            if (!nonotes)
            {
                sprintf(
                  temp,
                  "-=> Note: %s from %s by " PROGNAME " " VERSION "\r\r",
                  mode == MOVE ? "Moved" : "Copied", curarea->tag);

                totlen = totlen + strlen(temp);
            }
            else
            {
                memset(temp, '\0', sizeof(temp));
            }

            origlen = totlen;

            header->attr = attr;
            header->replyto = 0;
            memset(&header->replies, '\0', 40);

            /* Lets see if we want to add an AREA kludge */

            if (addareakludge && stristr(ctxt, "AREA:") == NULL)
            {
                /* None found, add one.. */
                sprintf(errortext, "\01AREA: %s", curarea->tag);
                strcat(ctxt, errortext);
                ctrllen = strlen(ctxt) + 1;
            }

            msghandle = MsgOpenMsg(toarea, MOPEN_CREATE, 0);
            
            if (msghandle == NULL)
            {
                fprintf(stderr, "Error opening message for write!\n");
                goto close_all;
            }

            if (MsgWriteMsg(msghandle, 0, header, temp, strlen(temp), totlen, ctrllen, ctxt) == -1)
            {
                fprintf(stderr, "Error writing message!\n");
            }

            if (MsgWriteMsg(msghandle, 1, 0L, msgtxt, origlen, totlen, 0L, 0L) == -1)
            {
                fprintf(stderr, "Error writing message!\n");
            }

            if (MsgCloseMsg(msghandle) == -1)
            {
                fprintf(stderr, "Problem closing message!\n");
            }

        }

        /* End " move or copy" */

        if (mode == LIST)
        {
            AddMessage(curarea->tag, msgno, header->from, header->subj);
        }

close_all:
        if (ctxt)
        {
            free(ctxt);
        }
        
        if (msgtxt)
        {
            free(msgtxt);
        }
        
        if (header)
        {
            free(header);
        }

        thismsg = thismsg->next;
    }

    if (MsgUnlock(toarea) == -1)
    {
        fprintf(stderr, "Error unlocking WIMM area!\n");
    }

    if (MsgCloseArea(toarea) == -1)
    {
        fprintf(stderr, "Error closing WIMM area!\n");
    }
}

static long GetLast(AREA * area, MSGA * areahandle)
{
    int lrfile, bytes_read, lastint;
    long last, lr;
    char temp[130];

    sprintf(temp, "%s\\lastread", area->dir);

#ifdef __UNIX__
    lrfile = open(temp, O_BINARY | O_RDONLY);
#else
    lrfile = sopen(temp, O_BINARY | O_RDONLY, SH_DENYNO);
#endif
    
    if (lrfile == -1)
    {
        return 0;
    }

    bytes_read = read(lrfile, &lastint, sizeof lastint);
    close(lrfile);

    if (bytes_read == sizeof lastint)
    {
         last = (long)lastint;
    }
    else
    {
        return 0;
    }

    lr = MsgUidToMsgn(areahandle, last, UID_PREV);

    if (lr == 0)
    {
        lr = MsgUidToMsgn(areahandle, last, UID_NEXT);
        return lr > 0 ? lr - 1 : 0;
    }
    else
    {
        return lr;
    }
}

static void PersMsg(MSGA * areahandle, AREA * curarea)
{
    unsigned long curno, last, scanned = 0;
    MSGH *msghandle;
    XMSG *header;
    NAMELIST *curname;
    MSGLIST *firstmsg = NULL, *thismsg, *lastmsg = NULL;

    last = (scanfrom == ALL) ? 0 : GetLast(curarea, areahandle);

    if (last >= MsgGetHighMsg(areahandle))
    {
        printf("No new messages...\n");
        return;
    }

    for (curno = last + 1; curno <= MsgGetHighMsg(areahandle); curno++)
    {
    	msghandle = MsgOpenMsg(areahandle, MOPEN_READ, curno);
    	
        if (msghandle == NULL)
        {
            /* open failed, try next message */
            continue;
        }

        header = calloc(1, sizeof(XMSG));

        if (MsgReadMsg(msghandle, header, 0L, 0L, NULL, 0L, NULL) != (dword) -1)
        {
            scanned++;
            curname = firstname;
            while (curname)
            {
                /* Try all names in list */

                if (strcmpi(header->to, curname->name) == 0)
                {
                    /* Yep! Match... */

                    thismsg = malloc(sizeof(MSGLIST));
                    if (firstmsg == NULL)
                    {
                        firstmsg = thismsg;
                    }
                    else
                    {
                        lastmsg->next = thismsg;
                    }
                    
                    thismsg->id = MsgMsgnToUid(areahandle, curno);
                    thismsg->next = NULL;
                    lastmsg = thismsg;
                }
                curname = curname->next;  /* Select next "alias" */
            }
        }

        free(header);
        MsgCloseMsg(msghandle);
    }

    printf("Scanned %ld messages.\n", scanned);

    /* Found any personal messages? */

    if (firstmsg)
    {
        ProcessMessages(areahandle, firstmsg, curarea);

        while (firstmsg)
        {
            /* Give mem for msglist back */
            thismsg = firstmsg;
            firstmsg = firstmsg->next;
            free(thismsg);
        }
    }
}

static dword GetLastread(char *lastname)
{
    dword lastread;
    int last;

#ifdef __UNIX__
    last = open(lastname, O_RDONLY | O_BINARY);
#else
    last = sopen(lastname, O_RDONLY | O_BINARY, SH_DENYNO);
#endif
    
    if (last == -1)
    {
        return 0;
    }

    if (read(last, &lastread, sizeof lastread) != sizeof lastread)
    {
         lastread = 0;
    }

    close(last);

    return lastread;
}

#ifndef _MSC_VER

static long filelength(int handle)
{
    long old_pos, new_pos;
    old_pos = tell(handle);
    if (old_pos == -1)
    {
    	return -1;
    }
    new_pos = lseek(handle, 0, SEEK_END);
    lseek(handle, old_pos, SEEK_SET);
    return new_pos;
}

#endif

static void PersSquish(AREA * curarea)
{
    struct my_idx *l, *idxptr;
    long total, scanned = 0;
    NAMELIST *curname;
    MSGLIST *firstmsg = NULL, *thismsg, *lastmsg = NULL;
    MSGA *areahandle;
    int index;
    char idxname[200], lastname[200];
    unsigned bytes;
    dword lastread = (dword) -1;
    dword lastuid = 0;
    int stop;

#define CHUNKSIZE 1000

    sprintf(idxname, "%s.sqi", curarea->dir);
    sprintf(lastname, "%s.sql", curarea->dir);

#ifdef __UNIX__
    index = open(idxname, O_RDONLY | O_BINARY);
#else
    index = sopen(idxname, O_RDONLY | O_BINARY, SH_DENYNO);
#endif
    
    if (index == -1 || filelength(index) == 0L)
    {
        /* empty area */

        printf("Empty area...\n");

        if (index != -1)
        {
            close(index);
        }

        return;
    }

    if (scanfrom == ALL)
    {
        idxmem.last = 0;
    }

    idxptr = calloc(1, CHUNKSIZE * sizeof(struct my_idx));

    lseek(index, 0L, SEEK_SET);

    stop = 0;

    while ((bytes = read(index, idxptr,
      (unsigned)CHUNKSIZE * sizeof(struct my_idx))) > 0)
    {
        if (stop)
        {
            break;
        }

        total = (long) bytes / sizeof(struct my_idx);

        if (total < 1)
        {
            break;
        }

        l = idxptr;

        while (total)
        {
            total--;

            if (((int) l->umsgid == -1) || (l->umsgid < lastuid))
            {
                stop = 1;
                break;
            }

            scanned++;

            curname = firstname;
            
            while (curname)
            {
                /* Try all names in list */

                if ((int) l->hash == curname->hash)
                {
                    if ((int) lastread == -1)
                    {
                        lastread = GetLastread(lastname);
                    }

                    if (scanfrom == LASTREAD && l->umsgid <= lastread)
                    {
                        curname = curname->next;
                        continue;
                    }

                    thismsg = malloc(sizeof(MSGLIST));

                    if (firstmsg == NULL)
                    {
                        firstmsg = thismsg;
                    }
                    else
                    {
                        lastmsg->next = thismsg;
                    }

                    thismsg->id = l->umsgid;
                    thismsg->next = NULL;
                    lastmsg = thismsg;
                }
                
                curname = curname->next;  /* Select next "alias" */
            }
            l++;
        }
    }

    printf("Scanned %ld index entries.\n", scanned);

    free(idxptr);

    close(index);

    if (firstmsg)
    {
        areahandle = MsgOpenArea(curarea->dir, MSGAREA_NORMAL, curarea->base);

        if (!areahandle)
        {
            fprintf(stderr, "Error opening %s! (MSGAPI returned NULL-handle)\n", curarea->dir);
            return;
        }

        ProcessMessages(areahandle, firstmsg, curarea);

        if (MsgCloseArea(areahandle) == -1)
        {
            fprintf(stderr, "Error closing Area %s!\n", curarea->tag);
        }

        while (firstmsg)
        {
            thismsg = firstmsg;
            firstmsg = firstmsg->next;
            free(thismsg);
        }
    }
}

static void DoScan(AREA * curarea)
{
    MSGA *areahandle;

    strcpy(currentarea, curarea->tag);

    if (curarea->status & SCANNED)
    {
        return;
    }

    curarea->status |= SCANNED;

    if (curarea->status & PASSTHRU)
    {
        return;
    }

    if (curarea->status & EXCLUDE)
    {
        return;
    }

    printf("%-40.40s  ", curarea->tag);

    if (curarea->base == MSGTYPE_SDM)
    {
    	/* Scan a *.MSG area */

        areahandle = MsgOpenArea(curarea->dir, MSGAREA_NORMAL, curarea->base);

        if (!areahandle)
        {
            if (msgapierr == MERR_NOENT)
            {
                printf("Empty area...\n");
            }
            else
            {
                fprintf(stderr, "Error opening %s! (MSGAPI returned NULL-handle)\n", curarea->dir);
            }

            return;
        }

        PersMsg(areahandle, curarea);

        if (MsgCloseArea(areahandle) == -1)
        {
            fprintf(stderr, "Error closing area %s!\n", curarea->tag);
        }
    }
    else
    {
        /* Squish area */
        PersSquish(curarea);
    }
}

static void ScanAllAreas(AREA * first)
{
    AREA *curarea;

    for (curarea = first; curarea; curarea = curarea->next)
    {
        DoScan(curarea);
    }
}

static AREA *GetArea(AREA * first, char *line)
{
    AREA *current;

    if (line[strlen(line) - 1] == '\n')
    {
        line[strlen(line) - 1] = '\0';
    }

    for (current = first; current; current = current->next)
    {
        if (stricmp(line, current->tag) == 0)
        {
            return current;
        }
    }

    return NULL;
}

static void ScanLogAreas(AREA * first)
{
    AREA *curarea;
    FILE *tosslog;
    char line[FGETS_BUFSIZE];

    tosslog = fopen(TossLogFile, "r");

    if (tosslog == NULL)
    {
        printf("Can't open %s!\n", TossLogFile);
    }
    else
    {
        printf("Scanning areas found in %s\n", TossLogFile);

        while ((getline(line, sizeof line, tosslog)) != NULL)
        {
            if (strlen(line) < 1)
            {
                continue;       /* Too short */
            }

            curarea = GetArea(first, line);

            if (curarea == NULL)
            {
                printf("Unknown area: %s\n", line);
                continue;
            }

            DoScan(curarea);
        }

        fclose(tosslog);
    }

    /* Look for any Forced but unscanned areas */

    curarea = firstarea;        

    while (curarea)
    {
        if (curarea->status & FORCED && !(curarea->status & SCANNED))
        {
            DoScan(curarea);
        }
        curarea = curarea->next;
    }
}

static void MakeList(void)
{
    char *msgtxt, temp[256], lastarea[30];
    dword totlen;
    MSGA *toarea;
    XMSG *header;
    MSGH *msghandle;
    PERSMSGLIST *curmsg;
    time_t now;
    union stamp_combo combo;
    struct tm *tmdate;

    foundmail = 1;

    strcpy(lastarea, "none");

    toarea = MsgOpenArea(LocalArea, MSGAREA_CRIFNEC, (word) (LocalType | MSGTYPE_LOCAL));

    if (toarea == NULL)
    {
        fprintf(stderr, "Error opening LocalArea (%s)!\n", LocalArea);
    }

    if (MsgLock(toarea) == -1)
    {
        fprintf(stderr, "Error locking local area (%s)!\n", LocalArea);
    }

    header = calloc(1, sizeof(XMSG));

    strcpy(header->from, "WIMM");
    strcpy(header->to, firstname->name);
    strcpy(header->subj, "Your mail!");

    header->attr = attr;

    time(&now);
    tmdate = localtime(&now);
    header->date_written = header->date_arrived = (TmDate_to_DosDate(tmdate, &combo))->msg_st;

    msghandle = MsgOpenMsg(toarea, MOPEN_CREATE, 0);
    
    if (msghandle == NULL)
    {
        fprintf(stderr, "Error opening message for writing!\n");
    }

    msgtxt = calloc(1, 16000);

    curmsg = firstpers;

    strcpy(msgtxt, "You have the following personal messages:\r");

    while (curmsg)
    {
        if (strcmpi(lastarea, "none") == 0 || strcmpi(lastarea, curmsg->tag) != 0)
        {
            sprintf(temp, "\rArea: %s\r\r", curmsg->tag);
            strcat(msgtxt, temp);
        }

        sprintf(temp, "#%4.4lu From: %26s (%40s)\r", curmsg->number, curmsg->from, curmsg->subject);

        if (strlen(temp) > 78)
        {
            temp[79] = '\0';
        }

        strcat(msgtxt, temp);
        strcpy(lastarea, curmsg->tag);

        if (strlen(msgtxt) > 15500)
        {
            strcat(msgtxt, "\rText too long, message aborted..\r");
            goto write_it;
        }
        curmsg = curmsg->next;
    }

    strcat(msgtxt, "\r\r--- " PROGNAME " " VERSION "\r * Origin:            (c) 1994  Gerard van Essen             (2:281/527)\r");

write_it:
    totlen = strlen(msgtxt);

    if (MsgWriteMsg(msghandle, 0, header, msgtxt, totlen, totlen, 0L, 0L) == -1)
    {
        fprintf(stderr, "Error writing message!\n");
    }

    MsgCloseMsg(msghandle);

    free(header);
    free(msgtxt);

    MsgUnlock(toarea);

    MsgCloseArea(toarea);

    curmsg = firstpers;

    while (curmsg)
    {
        firstpers = curmsg;
        curmsg = curmsg->next;
        free(firstpers);
    }
}

static void ScanAreas(AREA * first)
{
    if (TossLogFile[0] == 0)
    {
        ScanAllAreas(first);
    }
    else
    {
        ScanLogAreas(first);
    }

    if (firstpers != NULL)
    {
        MakeList();
    }
}

/* Show command line parms if I can't make sense out of them */

static void showparms(void)
{
    puts(
      "This program is free software; you can redistribute it and/or modify\n"
      "it under the terms of the GNU General Public License as published by\n"
      "the Free Software Foundation; either version 2 of the License, or\n"
      "(at your option) any later version.\n"
      "\n"
      "This program is distributed in the hope that it will be useful,\n"
      "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
      "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
      "GNU General Public License for more details.\n"
      "\n"
      "You should have received a copy of the GNU General Public License\n"
      "along with this program; if not, write to the Free Software\n"
      "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA."
    );

    puts(
      "\n"
      "Usage: wimm [-Cconfigfile] [-Fechotoss.log]\n"
      "\n"
      "Example: wimm -Cc:\\wimm\\mycfg.cfg -Fc:\\squish\\echotoss.log"
    );
    exit(0);
}

static void readparms(int argc, char *argv[])
{
    int i;
    char *p;

    for (i = 1; i < argc; i++)
    {
        p = argv[i];
        if (*p == '-' || *p == '/')
        {
            switch (tolower(*(++p)))
            {
            case 'c':
                /* config file */
                strcpy(cfgname, ++p);
                break;

            case 'f':
                /* echotoss.log */
                strcpy(TossLogFile, ++p);
                break;

            default:
                showparms();
            }
        }
        else
        {
            showparms();
        }
    }
}

static void dealloc_areas(AREA * first)
{
    AREA *ptr1, *ptr2;

    ptr1 = first;

    while (ptr1)
    {
        ptr2 = ptr1->next;
        free(ptr1);
        ptr1 = ptr2;
    }
}

static void dealloc_names(NAMELIST * first)
{
    NAMELIST *ptr1, *ptr2;

    ptr1 = first;
    while (ptr1)
    {
        ptr2 = ptr1->next;
        free(ptr1);
        ptr1 = ptr2;
    }
}

int main(int argc, char **argv)
{
    time_t begin, end;
    struct _minf minf;
    char temp[80];

    time(&begin);

    puts(
      PROGNAME " " VERSION "\n"
      "Copyright (c) 1994 Gerard van Essen (2:281/527)\n"
      "Copyright (c) 2002-2003 Andrew Clarke (3:633/267)\n"
      "Build date: " __DATE__ " " __TIME__ "\n"
    );

    minf.def_zone = 0;          /* set default zone */
    minf.req_version = 0;       /* level 0 of the MsgAPI */
    MsgOpenApi(&minf);          /* init the MsgAPI  */

    readparms(argc, argv);

    GetConfig(cfgname);

    if (LogFile[0] != 0)
    {
#ifndef __UNIX__
    	logfd = sopen(LogFile, O_CREAT | O_APPEND | O_RDWR, SH_DENYWR, S_IWRITE | S_IREAD);
#else
        logfd = open(LogFile, O_CREAT | O_APPEND | O_RDWR, S_IWRITE | S_IREAD);
#endif

        if (logfd == -1)
        {
            fprintf(stderr, "Can't open logfile! (%s)\n", LogFile);
            dolog = 0;
        }
        else
        {
            dolog = 1;
            sprintf(temp, "Begin, " PROGNAME " " VERSION);
            logit(temp, 1);
        }
    }

    ScanAreas(firstarea);

    time(&end);

    printf("Done! Total runtime: %d seconds.\n", (int) (end - begin));

    sprintf(temp, "End, " PROGNAME " " VERSION " ");

    logit(temp, 1);

    if (dolog)
    {
        write(logfd, "\n", strlen("\n"));
        close(logfd);
    }

    MsgCloseApi();

    dealloc_areas(firstarea);
    dealloc_names(firstname);

    return foundmail;
}
