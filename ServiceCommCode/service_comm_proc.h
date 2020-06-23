#ifndef SERVICE_COMM_PROC_H
#define	SERVICE_COMM_PROC_H

#include "../BootServer/global.h"

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "BootServer.lib")
#endif

#ifdef __cplusplus
extern "C" {
#endif

__declspec_dllexport int loadClusterTableFromJsonData(struct ClusterTable_t* table, const char* data);

#ifdef __cplusplus
}
#endif

#endif // !SERVICE_COMM_PROC_H
