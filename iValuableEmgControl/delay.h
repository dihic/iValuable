#ifndef DELAY

#ifdef __cplusplus
extern "C" {
#endif
	
extern void SysCtlDelay(unsigned long ulCount);
#define DELAY(us) SysCtlDelay((SystemCoreClock/6000000)*(us)) //SystemFrequency

#ifdef __cplusplus
}
#endif
	
#endif
