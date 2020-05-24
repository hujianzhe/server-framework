#ifndef SERVICE_CENTER_HANDLER_H
#define	SERVICE_CENTER_HANDLER_H

int loadClusterNode(const char* data);

void reqClusterList_http(UserMsg_t* ctrl);
void reqChangeClusterNode_http(UserMsg_t* ctrl);
void reqClusterList(UserMsg_t* ctrl);

#endif // !SERVICE_CENTER_HANDLER_H
