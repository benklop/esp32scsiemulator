#ifndef SVC_SCREEN_H
#define SVC_SCREEN_H

void screenInit();
void screenStatus(const char *st);
void screenCardSize(uint32_t blocks);
void screenRefresh();

#endif /* SVC_SCREEN_H */
