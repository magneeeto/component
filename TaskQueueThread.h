#pragma once

#include <queue>

#include "AutoLock.h"

typedef int FNCallBack(void *pParam);

typedef struct tagTask
{
	FNCallBack *pFunc;
	void* param;
}stTask, *LPTask;

//等增加任务类，可继承实现任务的取消甚至暂停继续等
class CTaskQueueThread
{
public:
	CTaskQueueThread(LPCSTR szThreadName = nullptr);
	CTaskQueueThread(FNCallBack *pFunc, void* param, bool bSingleTask = true, LPCSTR szThreadName = nullptr);
	~CTaskQueueThread(void);

	//返回值表示是否马上执行，即任务队列中只有当前一个任务
	BOOL PushTask(FNCallBack *pFunc, void* param);
	bool ClearTask(bool bExit = true);

	BOOL Start();

	void Terminate();

	HANDLE GetThreadHandle();//可用于WaitForSingleObject
	
	bool isRunning();
	void Wait(); //等待线程结束

protected:

	static unsigned int __stdcall Run(void *param);
	void ExecuteTasks();

	std::queue<stTask> m_queueFnCallBack;
	CCriticalSection m_csQueueFnCallBack;

	HANDLE m_hThread;

	bool m_bSingleTask;
	HANDLE m_hEvent;
	bool m_bExit;
	std::string m_strThreadName;

	static int m_nID;
};
