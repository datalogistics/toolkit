#ifndef WARMER_MSG_QUEUE_H_
#define WARMER_MSG_QUEUE_H_


/***---Prototypes---***/
int LoDNMsqQueueInit(Dllist paths);
int getExnodeCommandsFromQueue(int queueID, Dllist addExnodesList,
                               Dllist removeExnodesList, int wait);
int LoDNMsgQueueKill(int queueID);

#endif
