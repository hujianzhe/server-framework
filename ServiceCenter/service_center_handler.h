#ifndef SERVICE_CENTER_HANDLER_H
#define	SERVICE_CENTER_HANDLER_H

#include "cmd.h"

void reqClusterList_http(UserMsg_t* ctrl);
void reqClusterList(UserMsg_t* ctrl);
void reqClusterLogin(UserMsg_t* ctrl);
void reqClusterConnectLogin(UserMsg_t* ctrl);
void reqClusterTellSelf(UserMsg_t* ctrl);

#endif // !SERVICE_CENTER_HANDLER_H
