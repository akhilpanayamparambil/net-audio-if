#include "integer.h"

void StartTask (DWORD* tobj, void(*task)(void), void* stack);
void DispatchTask (DWORD* tobj);
void Sleep (SHORT tmr);
void TaskTimerproc (void);

