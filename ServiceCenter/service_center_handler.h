#ifndef SERVICE_CENTER_HANDLER_H
#define	SERVICE_CENTER_HANDLER_H

int loadClusterNode(const char* data);

void reqClusterList_http(TaskThread_t*, UserMsg_t* ctrl);
void reqChangeClusterNode_http(TaskThread_t*, UserMsg_t* ctrl);
void reqDistributeClusterNode_http(TaskThread_t*, UserMsg_t* ctrl);
void reqClusterList(TaskThread_t*, UserMsg_t* ctrl);

#endif // !SERVICE_CENTER_HANDLER_H
