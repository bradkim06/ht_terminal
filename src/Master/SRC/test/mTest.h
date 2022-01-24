#ifndef __MODEM_TEST_H__
#define __MODEM_TEST_H__

void TEST_initModem(int taskId);
void TEST_modem();
void TEST_modemResponse();
void TEST_pmodem();
void TEST_prdeui();
void TEST_pwdeui(char *p);
void TEST_praeui();
void TEST_pwaeui(char *p);
void TEST_prakey();
void TEST_pwakey(char *p);
BOOL TEST_writeAppKeyUsingDEUI();
void TEST_pwserver(char *ip, int port, char *serviceCode);
void TEST_prserver();
void TEST_pwfota(char *ip, int port, int interval);
void TEST_prfota();
void TEST_prnbid();
void TEST_pnbctx();

#endif
