
#ifndef COMMON_h
#define COMMON_h

static char  rcsid_common_h [ ] =
         "$Id: common.h,v 1.1 2001-10-12 23:29:07 brianb Exp $";
static void *no_unused_common_h_warn[]={rcsid_common_h, no_unused_common_h_warn};

extern char PASSWORD[512];
extern char USER[512];
extern char SERVER[512];
extern char DATABASE[512];

#endif
