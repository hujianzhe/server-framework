#ifndef SERVICE_CENTER_HANDLER_H
#define	SERVICE_CENTER_HANDLER_H

#include "cmd.h"

int reqClusterList_http(UserMsg_t* ctrl);
int reqClusterLogin(UserMsg_t* ctrl);

#endif // !SERVICE_CENTER_HANDLER_H
