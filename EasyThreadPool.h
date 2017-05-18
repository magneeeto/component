#ifndef CLOUDROOM_LOGIC_EASYTHREADPOOL_H
#define CLOUDROOM_LOGIC_EASYTHREADPOOL_H

#include <queue>
#include "Common/AutoLock.h"
#include "TransStruct.h"

//简易线程池思想+优先任务队列 可根据当前任务状态动态增减任务线程数
//数目成员包括：初始线程数 最多线程数 最少空闲线程数 最多空闲线程数
//原则上接到任务从空闲线程队列取线程执行，取了后判断最多线程数与最少空闲线程数决定是否创建空闲线程
//线程执行从空闲切换到繁忙，执行完后继续从任务队列取任务执行，当任务队列空了再放置空闲队列
//放置到空闲队列后，判断空闲队列最多数选择干掉空闲线程

///////////////////////////////////////////////////////////////////////////////////////////////
//优先队列不允许遍历。。。但为了增加任务的取消移除。。。将就一下吧
template <typename _Ty>
class TravelablePQ : public std::priority_queue<_Ty>
{
public:
	bool removeElem(const _Ty& elem)
	{
		bool bRet = false;
		auto it = find_if(c.begin(), c.end(), [&](const _Ty& curElem)->bool
		{return elem == curElem;});
		if (it != c.end())
		{
			bRet = true;
			c.erase(it);
			push_heap(c.begin(), c.end(), comp);
		}

		return bRet;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////

//启动参数 可能得扩展 单独放个结构
struct stMTPQMgrStartUpPara
{
	unsigned int nMaxThrdNum;//最多线程数 busy + idle
	unsigned int nMinIdleThrdNum;//最少保留空闲线程数 
	unsigned int nMaxIdleThrdNum;//最多保留的空闲线程数

	bool bQuitImmd;//退出方式 干掉线程|等待执行正常退出

	stMTPQMgrStartUpPara()
	{
		nMaxThrdNum = 5;
		nMinIdleThrdNum = 1;
		nMaxIdleThrdNum = 4;

		bQuitImmd = false;
	}
};

class CEasyThreadPoolMgr
{
	class CWorkThread;
	friend class CWorkThread;
public:
	CEasyThreadPoolMgr(int nInitThrdNum = 2, stMTPQMgrStartUpPara* pPara = NULL);
	~CEasyThreadPoolMgr();

public:
	void ExecJob(stJob* job);
	void TerminateJob(LPCSTR szJobKey);

	void ClearJobs();
	
	bool IsJobsDone()
	{
		CAutoLock _lock(&m_csBusyThreads);
		return m_listBusyThreads.size() == 0;
	}
private:
	stJob* GetJob();
	CWorkThread* CreatWorkThread();

	bool MoveBusyToIdle(CWorkThread* pThread);//超过最大空闲数干掉一些 suspend
	bool MoveIdleToBusy(CWorkThread* pThread, stJob* pJob);//resume
	
	// 超过最大空闲，旋转几次再做删除，CPU消耗影响可以接受，若挂起到运行时钟周期消耗比较不好
	// 没有考虑好删除空闲个数，目前超过最大空闲删一半，有待分析，旋转次数也有待分析。。。
	void DeleteIdleThreads(/*int nDelNum = 1, */int nSpinTime = 500);
	
private:
	class CWorkThread
	{
	public:
		CWorkThread(const char* cstrThreadName);
		~CWorkThread();

	public:
		//mgr将thread从idle到busy中的调用 
		//自己在busy中不用，通过mgr获取排队任务处理
		void SetJob(stJob* pJob)
		{
			//不需要加锁，除了从空闲到繁忙mgr激活时才会从其他线程setJob
			//那时线程阻塞，自己不会操作job，其余都是只有线程自己才操作job
			//CAutoLock _lock(&m_csJob);
			m_pJob = pJob;

			m_bIsSuspend = false;
			if (ResumeThread(m_hThread) == -1)
			{
				RetrieveErrCall(_T("EasyThrdPool：SetJob"));
			}
		}

		const stJob* GetJob() const
		{
			return m_pJob;
		}

		HANDLE GetThreadHandle()
		{
			return m_hThread;
		}

		unsigned int GetThreadID()
		{
			return m_nThreadID;
		}

		void RegisteMgr(CEasyThreadPoolMgr* pMgr)
		{
			m_pThreadsMgr = pMgr;
		}

		//正常结束线程 并在结束前释放对象
		//但没那么及时，至少得完成当前任务。。。
		static void DeleteThread(CWorkThread* pThread)
		{
			if (pThread == NULL)
			{
				return;
			}

			pThread->m_bIsRun = false;
			if (pThread->m_bIsSuspend)
			{
				ResumeThread(pThread->m_hThread);
			}
		}
		
		void SetThrdSusp(bool bIsSusp = true)
		{
			m_bIsSuspend = bIsSusp;
		}

		bool IsRun()
		{
			return m_bIsRun;
		}
	
	private:
		static unsigned int __stdcall Run(void*);

	private:
		CEasyThreadPoolMgr* m_pThreadsMgr;
		HANDLE m_hThread;
		unsigned int  m_nThreadID;
		
		CCriticalSection m_csJob;
		stJob* m_pJob;

		bool m_bIsSuspend;//通过mgr操作，不用同步
		bool m_bIsRun;//通过控制run 结束线程 不用同步
	};

private:
	unsigned int m_nInitThrdNum;//初始线程数 
	unsigned int m_nMaxThrdNum;//最多线程数 busy + idle
	unsigned int m_nMinIdleThrdNum;//最少保留空闲线程数 
	unsigned int m_nMaxIdleThrdNum;//最多保留的空闲线程数 
	
	std::list<CWorkThread*> m_listBusyThreads;
	CCriticalSection m_csBusyThreads;
	std::list<CWorkThread*> m_listIdleThreads;
	CCriticalSection m_csIdleThreads;

	TravelablePQ<stJob*> m_TaskQueue;//job的内存管理在各workThread中做
	CCriticalSection m_csTaskQueue;

	int m_nCreatedThreadNum;

	bool m_bQuitImmd;//退出方式
	std::list<HANDLE> m_listQuitBusyThrd;//等待正常退出线程句柄
};

#endif //CLOUDROOM_LOGIC_EASYTHREADPOOL_H