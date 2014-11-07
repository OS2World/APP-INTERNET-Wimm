/*
 *  WIMM; Where Is My Mail; Personal mail scanner for Squish & *.MSG
 *
 *  See LICENCE for copyright details.
 *
 *  wimmcfg.c  Config file parsing module
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef __BORLANDC__
#include <dir.h>
#endif

#ifdef __UNIX__
#include <fcntl.h>
#endif

#ifndef MAXDRIVE
#define MAXDRIVE 3
#endif

#ifndef MAXDIR
#define MAXDIR 256
#endif

#include "msgapi.h"
#include "wimm.h"
#include "wimmcfg.h"

char SquishCfg[200];            /* Where is your Squish configfile? */
char LocalArea[200];            /* The name of the area to put output in */
char LogFile[200];
word LocalType;                 /* Type of local area (Squish or *.MSG) */
NAMELIST *firstname;            /* First name in list to search for */
AREALIST *exclude_first, *force_first;
AREA *firstarea;                /* First area in list of areas to search */
char AreasBBS[200];             /* Path and name of areas.bbs file */
AREA *lastarea = NULL;          /* Used in building list of areas */
int mode = COPY;                /* Mode of operation (list, move, copy) */
int markreceived = 1;           /* Should personal msgs found be marked
                                   received */
int scanfrom = LASTREAD;        /* Scan all msgs or from lastread? */
dword attr = 0L | MSGSCANNED | MSGPRIVATE | MSGSENT;
int nonotes;
int addareakludge = 0;

static char *Strip_Trailing(char *str, char strip)
{
    int x;

    if (str && *str && str[x = strlen(str) - 1] == strip)
    {
        str[x] = '\0';
    }

    return str;
}

/* Add an area from Squish.cfg to the linked list */

static void AddEchoArea(char *line)
{
    AREA *thisarea;
    char *rest_of_line, *dirptr, *tagptr;

    if (line == NULL)
    {
        return;
    }

    thisarea = calloc(1, sizeof(AREA));

    if ((tagptr = strtok(line, " \t\n")) == NULL)
    {
        free(thisarea);
        return;
    }

    if ((dirptr = strtok(NULL, " \t\n")) == NULL)
    {
        free(thisarea);
        return;
    }

    rest_of_line = strtok(NULL, "\n");

    strncpy(thisarea->dir, dirptr, 79);
    Strip_Trailing(thisarea->dir, '\\');
    strncpy(thisarea->tag, tagptr, 59);

    if (strcmpi(thisarea->dir, LocalArea) == 0)
    {
        free(thisarea);
        return;
    }

    if (rest_of_line == NULL)
    {
        thisarea->base = MSGTYPE_SDM;
    }
    else if ((rest_of_line != NULL) && (strchr(rest_of_line, '$') != NULL))
    {
        thisarea->base = MSGTYPE_SQUISH;
    }
    else
    {
        thisarea->base = MSGTYPE_SDM;
    }

    if ((rest_of_line != NULL) && (strstr(rest_of_line, "-0") != NULL))
    {
        thisarea->status |= PASSTHRU;
    }

    thisarea->next = NULL;

    if (firstarea == NULL)
    {
        /* first name */
        firstarea = thisarea;
    }
    else
    {
        /* establish link */
        lastarea->next = thisarea;
    }

    lastarea = thisarea;

}

static void AddName(char *name)
{
    char temp[40], *charptr, *tempptr;
    NAMELIST *thisname;
    static NAMELIST *lastname;

    if (name == NULL)
    {
        return;
    }

    memset(temp, '\0', sizeof(temp));
    tempptr = temp;
    charptr = name;

    if (strchr(name, '"') == NULL || strchr(name, '"') == strrchr(name, '"'))
    {
        fprintf(stderr, "Illegal name format in %s\n", name);
        return;
    }

    while (*charptr++ != '"') /* nothing */ ;  /* skip leading space */

    while (*charptr != '"')
    {
        /* Copy chars */
        *tempptr++ = *charptr++;
    }

    thisname = malloc(sizeof(NAMELIST));
    strcpy(thisname->name, temp);
    thisname->hash = SquishHash((byte *) temp);
    thisname->next = NULL;

    if (firstname == NULL)
    {
        /* first name */
        firstname = thisname;
    }
    else
    {
        /* establish link */
        lastname->next = thisname;
    }

    lastname = thisname;
}

static void addforced(char *areaname)
{
    static AREALIST *lastarea;
    AREALIST *thisarea;

    if (areaname == NULL)
    {
        return;
    }

    if (strlen(areaname) < 2)
    {
        /* Too short to my liking */
        return;
    }

    thisarea = calloc(1, sizeof(AREALIST));
    strncpy(thisarea->tag, areaname, 59);
    thisarea->next = NULL;

    if (force_first == NULL)
    {
        /* first name */
        force_first = thisarea;
    }
    else
    {
        /* establish link */
        lastarea->next = thisarea;
    }

    lastarea = thisarea;
}

static void addexcluded(char *areaname)
{
    static AREALIST *lastarea;
    AREALIST *thisarea;

    if (areaname == NULL)
    {
        return;
    }

    if (strlen(areaname) < 2)
    {
        /* Too short to my liking */
        return;
    }

    thisarea = malloc(sizeof(AREALIST));
    strncpy(thisarea->tag, areaname, 59);
    thisarea->next = NULL;

    if (exclude_first == NULL)
    {
        /* first name */
        exclude_first = thisarea;
    }
    else
    {
        /* establish link */
        lastarea->next = thisarea;
    }

    lastarea = thisarea;
}

/* Set Squish or *.MSG as type of an area */

void SetType(char *value)
{
    if (value == NULL)
    {
        return;
    }

    if (stricmp(value, "SQUISH") == 0)
    {
        LocalType = MSGTYPE_SQUISH;
    }
    else
    {
        LocalType = MSGTYPE_SDM;
    }
}

/* Get operating mode for WIMM */

void GetMode(char *value)
{
    if (value == NULL)
    {
        return;
    }

    if (strcmpi(value, "LIST") == 0)
    {
        mode = LIST;
    }
    else if (strcmpi(value, "MOVE") == 0)
    {
        mode = MOVE;
    }
    else if (strcmpi(value, "COPY") == 0)
    {
        mode = COPY;
    }
}

/* Should messages be marked received? */

static void DoMark(char *value)
{
    if (value == NULL)
    {
        return;
    }

    if (stricmp(value, "YES") == 0)
    {
        markreceived = 1;
    }
    else
    {
        markreceived = 0;
    }
}

static void getattributes(char *attribs)
{
    /* We're specifying attribs, so zap all out first */

    attr = 0L;

    while (*attribs)
    {
        switch (*attribs++)
        {
        case 'P':
            attr |= MSGPRIVATE;
            break;
        case 'S':
            attr |= MSGSENT;
            break;
        case 'C':
            attr |= MSGSCANNED;
            break;
        case 'L':
            attr |= MSGLOCAL;
            break;
        }
    }
}

static void Analyse(char *line, int readwimm)
{
    char *keyword, *value;
    AREA *prev;

    keyword = strtok(line, " \n\r\t\0");  /* Get keyword */

    if ((keyword == NULL) || (keyword[0] == ';'))
    {
        /* It's a comment! */
        return;
    }

    strupr(keyword);

    if ((value = strtok(NULL, "\r\n")) == NULL)
    {
        /* Get value */
        return;
    }

    if ((strcmp(keyword, "ECHOAREA") == 0) || (strcmp(keyword, "BADAREA") == 0))
    {
        AddEchoArea(value);
        return;
    }

    if (strcmp(keyword, "DUPEAREA") == 0)
    {
        prev = lastarea;        /* what's last now */
        AddEchoArea(value);
        if ((prev != lastarea) && (lastarea != NULL))
        {
            /* Was one added? - Are there any areas at all? */
            lastarea->status |= EXCLUDE;
        }
        return;
    }

    if (strcmp(keyword, "NAME") == 0)
    {
        AddName(value);
        return;
    }

    /* End of keywords that may contain spaces.... */

    if ((value = strtok(value, " \t")) == NULL)
    {
        return;
    }
    else if (strcmp(keyword, "FORCE") == 0)
    {
        addforced(value);
    }
    else if (strcmp(keyword, "EXCLUDE") == 0)
    {
        addexcluded(value);
    }
    else if (strcmp(keyword, "SQUISHCFG") == 0)
    {
        strcpy(SquishCfg, value);
    }
    else if (strcmp(keyword, "WIMMAREA") == 0)
    {
        strcpy(LocalArea, Strip_Trailing(value, '\\'));
    }
    else if (strcmp(keyword, "WIMMTYPE") == 0)
    {
        SetType(value);
    }
    else if (strcmp(keyword, "AREASBBS") == 0)
    {
        strcpy(AreasBBS, value);
    }
    else if (strcmp(keyword, "LOG") == 0)
    {
        strcpy(LogFile, value);
    }
    else if (strcmp(keyword, "MODE") == 0)
    {
        GetMode(value);
    }
    else if (strcmp(keyword, "MARKRECEIVED") == 0)
    {
        DoMark(value);
    }
    else if (strcmp(keyword, "SCANFROM") == 0)
    {
        if (strcmpi(value, "ALL") == 0)
        {
            scanfrom = ALL;
        }
    }
    else if (strcmp(keyword, "ATTRIBUTES") == 0)
    {
        getattributes(value);
    }
    else if (strcmp(keyword, "NOTES") == 0)
    {
        if (strcmpi(value, "NO") == 0)
        {
            nonotes = 1;
        }
    }
    else if (strcmp(keyword, "ADDAREAKLUDGE") == 0)
    {
        if (strcmpi(value, "YES") == 0)
            addareakludge = 1;
    }
    else if (readwimm == 1)
    {
        fprintf(stderr, "Unknown keyword: %s!\n", keyword);
    }
}

/* Read the squish.cfg file and get Echoareas out of it */

char *getline(char *dest, int n, FILE *fp)
{
    char *p;

    if (n < 1 || dest == NULL || fp == NULL)
    {
        return NULL;
    }

    if (fgets(dest, n, fp) == NULL)
    {
        return NULL;
    }

    if (*dest != '\0')
    {
        p = dest + strlen(dest) - 1;

        if (*p == '\n')
        {
            *p = '\0';
        }
    }

    return dest;
}

static void ReadSquishCfg(void)
{
    FILE *squishfile;
    char line[FGETS_BUFSIZE];

    squishfile = fopen(SquishCfg, "r");

    if (squishfile == NULL)
    {
        fprintf(stderr, "Can't open squish.cfg! (%s)\n", SquishCfg);
        return;
    }

    printf("Reading: %s\n", SquishCfg);

    while ((getline(line, sizeof line, squishfile)) != NULL)
    {
        Analyse(line, 0);
    }

    fclose(squishfile);
}

/* Here we add an area from a areas.bbs line to the list */

void AddAreasArea(char *line)
{
    AREA *thisarea, *curarea;
    char *dirptr, *tagptr;

    if (line == NULL)
    {
        return;
    }

    thisarea = calloc(1, sizeof(AREA));

    thisarea->base = MSGTYPE_SDM;

    while (line[0] == '$' || line[0] == '#')
    {

        if (line[0] == '$')
        {
            /* Squish area */

            thisarea->base = MSGTYPE_SQUISH;

            /* Skip first char ($) */
            line++;
        }

        if (line[0] == '#')
        {
            thisarea->status |= PASSTHRU;
            line++;
        }
    }

    if ((dirptr = strtok(line, " \t\n")) == NULL)
    {
        free(thisarea);
        return;
    }

    strcpy(thisarea->dir, dirptr);
    Strip_Trailing(thisarea->dir, '\\');

    if ((tagptr = strtok(NULL, " \t\n")) == NULL)
    {
        free(thisarea);
        return;
    }

    strncpy(thisarea->tag, tagptr, 59);

    thisarea->next = NULL;

    if (strcmpi(thisarea->dir, LocalArea) == 0)
    {
        free(thisarea);
        return;
    }

    curarea = firstarea;
    while (curarea)
    {
        if (strcmpi(curarea->tag, thisarea->tag) == 0)
        {
            /* Dupe! */

            /* Check to see if area is finally marked as being Squish... */

            if ((curarea->base != MSGTYPE_SQUISH) && (thisarea->base == MSGTYPE_SQUISH))
            {
                curarea->base = MSGTYPE_SQUISH;
            }

            /* .... or as passthru.. */

            if ((!(curarea->status & PASSTHRU)) && (thisarea->status & PASSTHRU))
            {
                curarea->status |= PASSTHRU;
            }

            free(thisarea);

            return;
        }
        curarea = curarea->next;
    }

    if (firstarea == NULL)
    {
        /* first name */
        firstarea = thisarea;
    }
    else
    {
        /* establish link */
        lastarea->next = thisarea;
    }

    lastarea = thisarea;
}

/* Read a Areas.bbs file and get the defined areas */

static void ReadAreasBBS(void)
{
    FILE *areasfile;
    char temppath[256], *p;
    int skip = 0;  /* To skip first line */
    char line[FGETS_BUFSIZE];

#ifndef __UNIX__
    char drive[MAXDRIVE], dir[MAXDIR];
#endif

    areasfile = fopen(AreasBBS, "r");

    if (areasfile == NULL)
    {
#ifdef __UNIX__
        sprintf(temppath, "%s", AreasBBS);
#else
#ifdef __BORLANDC__
        fnsplit(SquishCfg, drive, dir, NULL, NULL);
#else
        _splitpath(SquishCfg, drive, dir, NULL, NULL);
#endif
        sprintf(temppath, "%s%s%s", drive, dir, AreasBBS);
#endif
        areasfile = fopen(temppath, "r");

        if (areasfile == NULL)
        {
            fprintf(stderr, "Can't open areas.bbs file! (%s)\n", AreasBBS);
            return;
        }
    }

    while ((getline(line, sizeof line, areasfile)) != NULL)
    {
        p = line;

        while (p && isspace(*p))
        {
            p++;
        }

        strcpy(line, p);

        if ((strlen(line) > 4) && (line[0] != ';') && (line[0] != '-') && skip++)
        {
            AddAreasArea(line);
        }
    }

    fclose(areasfile);
}

static void readconfig(char *cfgname)
{
    FILE *configfile;
    char line[FGETS_BUFSIZE];

    configfile = fopen(cfgname, "r");

    if (configfile == NULL)
    {
        fprintf(stderr, "Can't open configfile! (%s)\n", cfgname);
        return;
    }

    printf("Reading: %s\n", cfgname);

    while ((getline(line, sizeof line, configfile)) != NULL)
    {
        Analyse(line, 1);
    }

    fclose(configfile);

    if (SquishCfg[0] == '\0')
    {
        fprintf(stderr, "Warning: No squish.cfg specified!\n");
    }
    else
    {
        ReadSquishCfg();
    }

    if (AreasBBS[0] != '\0')
    {
        printf("Reading areas.bbs file: %s\n", AreasBBS);
        ReadAreasBBS();
    }
}

#ifndef _MSC_VER

static char *strrev(char *str)
{
    char *p1, *p2;

    if (str == NULL || *str == '\0')
    {
        return str;
    }

    p1 = str;
    p2 = str + strlen(str) - 1;

    while (p2 > p1)
    {
        *p1 ^= *p2;
        *p2 ^= *p1;
        *p1 ^= *p2;
        p1++;
        p2--;
    }

    return str;
}

static char *strchcase(char *str, int upper)
{
    char *p;
    p = str;
    if (p != NULL)
    {
        while (*p != '\0')
        {
            if (upper)
            {
                *p = (char)toupper(*p);
            }
            else
            {
                *p = (char)tolower(*p);
            }
            p++;
        }
    }
    return str;
}

static char *strupr(char *str)
{
    return strchcase(str, 1);
}

#endif

static int inlist(AREALIST * first, char *tag)
{
    char *charptr, *str1, *str2;
    AREALIST *areaptr = first;

    while (areaptr)
    {
        charptr = areaptr->tag;

        if (charptr[0] == '*')  /* starts w/ wildchart */
        {
            str1 = strdup(charptr + 1);
            strrev(str1);
            str2 = strdup(tag);
            strrev(str2);
            if (strnicmp(str1, str2, strlen(str1)) == 0)
            {
                free(str1);
                free(str2);
                return 1;
            }
            free(str1);
            free(str2);
        }
        else if (charptr[strlen(charptr) - 1] == '*')
        {
            str1 = strdup(charptr);
            str1[strlen(str1) - 1] = '\0';
            if (strnicmp(str1, tag, strlen(str1)) == 0)
            {
                free(str1);
                return 1;
            }
            free(str1);
        }
        else if (strcmpi(charptr, tag) == 0)
        {
            return 1;
        }

        areaptr = areaptr->next;
    }

    return 0;
}

static void check_status(void)
{
    AREA *atbc;

    atbc = firstarea;

    while (atbc)
    {
        if (exclude_first && inlist(exclude_first, atbc->tag))
        {
            atbc->status |= EXCLUDE;
        }

        if (force_first && inlist(force_first, atbc->tag))
        {
            atbc->status |= FORCED;
        }

        atbc = atbc->next;
    }
}

static void make_empty(AREALIST * first)
{
    AREALIST *ptr1, *ptr2;

    ptr1 = first;

    while (ptr1)
    {
        ptr2 = ptr1->next;
        free(ptr1);
        ptr1 = ptr2;
    }
}

void GetConfig(char *cfgname)
{
    /* Initialize & cleanup some vars, to be safe */

    memset(SquishCfg, '\0', sizeof(SquishCfg));
    memset(LocalArea, '\0', sizeof(LocalArea));
    memset(AreasBBS, '\0', sizeof(AreasBBS));
    memset(LogFile, '\0', sizeof(LogFile));

    LocalType = MSGTYPE_SDM;
    firstname = NULL;
    firstarea = NULL;
    exclude_first = NULL;
    force_first = NULL;
    nonotes = 0;

    readconfig(cfgname);

    if (exclude_first || force_first)
    {
        check_status();

        if (exclude_first)
        {
            make_empty(exclude_first);
        }

        if (force_first)
        {
            make_empty(force_first);
        }
    }
}

