#ifndef	BOOT_SERVER_CLUSTER_ACTION_H
#define	BOOT_SERVER_CLUSTER_ACTION_H

#include "cluster.h"

#ifdef __cplusplus
extern "C" {
#endif

ClusterNode_t* flushClusterNodeFromJsonData(struct ClusterTable_t* t, const char* json_data);
__declspec_dll struct ClusterTable_t* loadClusterTableFromJsonData(struct ClusterTable_t* t, const char* json_data, const char** errmsg);

#ifdef __cplusplus
}
#endif

#endif
