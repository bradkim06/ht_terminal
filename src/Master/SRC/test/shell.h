#ifndef __SHELL_H__
#define __SHELL_H__

#define MAX_COLUMN 100
#define MAX_HISTORY 4

#define CHAR_UP_ARROW 0x01
#define CHAR_DOWN_ARROW 0x02
#define CHAR_RIGHT_ARROW 0x03
#define CHAR_LEFT_ARROW 0x04

typedef struct {
	int wptr;
	int hptr; // history pointer
	char cmd[MAX_HISTORY][MAX_COLUMN];
} ShellCmdBuffer_t;

void SHELL_init();
void SHELL_run();
char SHELL_getChar();
int SHELL_gets(char *buf);
uint32 SHELL_decimal(char *p);
void SHELL_string(char *p, char *strbuf);

#endif
