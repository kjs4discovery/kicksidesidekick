/* Licence: TBD (see D017) */
/* Android's logging, which a few EAS files call on bad input: nothing to log to on the FM-1. */
#ifndef FW_CUTILS_LOG_H
#define FW_CUTILS_LOG_H
#define ALOGE(...) ((void)0)
#define ALOGW(...) ((void)0)
#define android_errorWriteLog(tag, subtag) ((void)0)
#endif
