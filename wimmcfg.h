#ifndef __WIMMCFG_H__
#define __WIMMCFG_H__

void GetConfig(char *cfgname);

extern NAMELIST *firstname;
extern AREA *firstarea;
extern AREALIST *exclude_first, *force_first;
extern char LocalArea[200];
extern char LogFile[200];
extern word LocalType;
extern int mode;
extern int scanfrom;
extern int markreceived;
extern dword attr;
extern int nonotes;
extern int addareakludge;

#endif
