#ifndef CMD_H
#define	CMD_H

enum {
	CMD_REQ_LOGIN_TEST = 1,
	CMD_RET_LOGIN_TEST = 2,

	CMD_REQ_TEST = 100,
	CMD_NOTIFY_TEST = 101,
	CMD_RET_TEST = 102,
	CMD_REQ_TEST_CALLBACK = 103,

	CMD_REQ_ParallelTest1 = 200,
	CMD_REQ_ParallelTest2 = 201,
};

#endif // !CMD_H
