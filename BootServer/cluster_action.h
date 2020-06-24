#ifndef	CLUSTER_ACTION_H
#define	CLUSTER_ACTION_H

#include "cluster.h"

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport ClusterNode_t* flushClusterNodeFromJsonData(struct ClusterTable_t* t, const char* json_data);
__declspec_dllexport const char* loadClusterTableFromJsonData(struct ClusterTable_t* t, const char* json_data);

#ifdef __cplusplus
}
#endif

#endif