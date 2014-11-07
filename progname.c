/*
 *  progname.c
 *
 *  Returns the name of the program.
 *
 *  Written in October 2002 by Andrew Clarke <mail@ozzmosis.com>
 *  for c-prog@yahoogroups.com and released to the public domain.
 *
 *  Normal usage would be:
 *
 *  printf("Usage: %s ...\n", ProgramName(argv[0], "yourprog"));
 *
 *  Where "yourprog" is used when argv[0] cannot be parsed.
 *
 *  #define PROGRAMNAME_STRIP_EXTENSION to strip the extention
 *  (eg. .exe) from the filename.
 *
 *  #define PROGRAMNAME_LOWERCASE to convert the program name to all
 *  lowercase.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *ProgramName(const char *argv0, char *def)
{
    static char *program = NULL;
    char *p;

    /* make a quick exit if we've been here before */

    if (program != NULL)
    {
        return program;
    }
    
    /* make a copy of the full filename */

    program = malloc(strlen(argv0) + 1);

    if (program == NULL)
    {
        /* malloc failed - we may as well return the default */

        return def;
    }

    strcpy(program, argv0);
    
    /* make program name point to the first character after the last \ */

    p = strrchr(program, '\\');

    if (p != NULL)
    {
        program = p + 1;
    }

    /* make program name point to the first character after the last / */

    p = strrchr(program, '/');

    if (p != NULL)
    {
        program = p + 1;
    }

#if PROGRAMNAME_STRIP_EXTENSION
    /* remove the extension from the filename */

    p = strrchr(program, '.');

    if (p != NULL)
    {
        *p = '\0'; 
    }
#endif
 
    if (*program == '\0')
    {
        /*
         *  somehow we ended up with an empty string!
         *  return the default instead
         */

        return def;
    }

#if PROGRAMNAME_LOWERCASE
    p = program;

    while (*p != '\0')
    {
        *p = tolower(*p);
        p++;
    }
#endif

    return program;
}

#ifdef TEST_PROGNAME

#ifndef unused
#define unused(x) ((void)(x))
#endif

int main(int argc, char **argv)
{
    unused(argc);
    printf("argv[0] == `%s`\n", argv[0]);
    printf("ProgramName returned `%s`\n", ProgramName(argv[0], "turnip"));
    return 0;
}

#endif
