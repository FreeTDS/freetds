#include <stdio.h>
#include <string.h>

char USER[512];
char SERVER[512];
char PASSWORD[512];
char DATABASE[512];

int read_login_info()
{
FILE *in;
char line[512];
char *s1, *s2;

	in = fopen("../../../PWD","r");
	if (!in) {
		fprintf(stderr,"Can not open PWD file\n\n");
		return 1;
	}
	while (fgets(line, 512, in)) {
		s1=strtok(line,"=");
		s2=strtok(NULL,"\n");
		if (!s1 || !s2) continue;
		if (!strcmp(s1,"UID")) {
			strcpy(USER,s2);
		} else if (!strcmp(s1,"SRV")) {
			strcpy(SERVER,s2);
		} else if (!strcmp(s1,"PWD")) {
			strcpy(PASSWORD,s2);
		} else if (!strcmp(s1,"DB")) {
			strcpy(DATABASE,s2);
		}
	}
	return 0;
}
