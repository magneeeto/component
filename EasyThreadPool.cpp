#include "stdafx.h"
#include "EasyThreadPool.h"
#include "TransStruct.h"

#define DEL_THRD(pThrd) CEasyThreadPoolMgr::CWorkThread::DeleteThread(pThrd)

//头文件前置声明 只能用指针装装样子 涉及到成员调用、子类继承会有问题
stJob::~stJob()
{	
	if(data)
		delete data;
}

CEasyThreadPoolMgr::CWorkThread::CWorkThread(const char* cstrThreadName)
{
	//SECURITY_ATTRIBUTES sa;
	//SECURITY_DESCRIPTOR sd;

	//sa.nLength = sizeof(sa);
	//sa.lpSecurityDescriptor = &sd;
	//sa.bInheritHandle = FALSE;

	//InitializeSecurityDescriptor(&sd, THREAD_SUSPEND_RESUME);
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, Run, this, CREATE_SUSPENDED, &m_nThreadID);
	//DWORD d = SuspendThread(m_hThread);
	::SetThreadName(m_nThreadID, cstrThreadName);

	m_pThreadsMgr = NULL;
	m_pJob = NULL;
	m_bIsRun = true;
	m_bIsSuspend = true;
}

CEasyThreadPoolMgr::CWorkThread::~CWorkThread()
{
	//::PostThreadMessage(m_nThreadID, WM_QUIT, 0, 0);
	//ExitThread(m_nThreadID);
	CloseHandle(m_hThread);

	if (m_pJob != NULL)
	{
		delete m_pJob;
	}
}

//自己不挂起自己 当任务队列空时 由manager挂起它
unsigned int CEasyThreadPoolMgr::CWorkThread::Run(void* data)
{
	CWorkThread* pThis = static_cast<CWorkThread*>(data);
	ASSERT(pThis);
	if (pThis != NULL)
	{
		while(pThis->m_bIsRun)
		{
			//不需要加锁，除了从空闲到繁忙mgr激活时才会从其他线程setJob
			//那时线程阻塞，自己不会操作job，其余都是只有线程自己才操作job
			{
				//CAutoLock _lock(&m_csJob);
				if (pThis->m_pJob != NULL)
				{
					pThis->m_pJob->callback(pThis->m_pJob->data);//具体业务实现函数不需要管理内存

					delete pThis->m_pJob;
					pThis->m_pJob = NULL;
				}
			}

			if (pThis->m_pJob == NULL && pThis->m_bIsRun)
			{
				pThis->m_pJob = pThis->m_pThreadsMgr->GetJob();
				if (pThis->m_pJob == NULL && pThis->m_bIsRun)
				{
					pThis->m_pThreadsMgr->MoveBusyToIdle(pThis);
				}
			}
		}

		delete pThis;
	}

	return 0;
}

CEasyThreadPoolMgr::CEasyThreadPoolMgr(int nInitThrdNum/* = 2*/, stMTPQMgrStartUpPara* pPara /*= NULL*/)
{
	m_nInitThrdNum = nInitThrdNum;
	m_nMaxThrdNum = pPara == NULL ? 5 : pPara->nMaxThrdNum;
	m_nMinIdleThrdNum = pPara == NULL ? 1 : pPara->nMinIdleThrdNum;
	m_nMaxIdleThrdNum = pPara == NULL ? 4 : pPara->nMaxIdleThrdNum;
	m_bQuitImmd = pPara == NULL ? false : pPara->bQuitImmd;

	m_nCreatedThreadNum = 0;
	
	while(nInitThrdNum--)
	{	
		CWorkThread* pWorkThread = CreatWorkThread();
		m_listIdleThreads.push_back(pWorkThread);
	}
}

CEasyThreadPoolMgr::~CEasyThreadPoolMgr()
{
	LD("enter");
	ClearJobs();
	for (auto it = m_listQuitBusyThrd.begin(); it != m_listQuitBusyThrd.end(); it++)
	{
		LD("begin wait");
		WaitForSingleObject(*it, 1000);
		LD("end wait");
	}
	LD("exit");
}

void CEasyThreadPoolMgr::ClearJobs()
{
	{
		auto TERM_THRD = [](CWorkThread* pThread)
		{
			::TerminateThread(pThread->GetThreadHandle(), 0);
			delete pThread;
		};

		CAutoLock _lock(&m_csBusyThreads);
		for (auto it = m_listBusyThreads.begin(); it != m_listBusyThreads.end(); it++)
		{
			m_bQuitImmd ? TERM_THRD(*it) : DEL_THRD(*it);
			m_bQuitImmd ? void() : m_listQuitBusyThrd.push_back((*it)->GetThreadHandle());
		}

		m_listBusyThreads.clear();
	}

	{
		CAutoLock _lock(&m_csIdleThreads);
		for (auto it = m_listIdleThreads.begin(); it != m_listIdleThreads.end(); it++)
		{
			DEL_THRD(*it);
		}
		
		m_listIdleThreads.clear();
	}
	
	CAutoLock _lock(&m_csTaskQueue);
	while(!m_TaskQueue.empty())
	{
		auto it = m_TaskQueue.top();
		m_TaskQueue.pop();
		delete it;
	}		
}

CEasyThreadPoolMgr::CWorkThread* CEasyThreadPoolMgr::CreatWorkThread()
{
	std::string strThreadName = "WorkThread No.";
	strThreadName += CI2A(m_nCreatedThreadNum++);
	CWorkThread* pWorkThread = new CWorkThread(strThreadName.c_str());
	pWorkThread->RegisteMgr(this);

	return pWorkThread;
}

bool CEasyThreadPoolMgr::MoveBusyToIdle(CWorkThread* pThread)
{
	if (pThread == NULL)
	{
		return false;
	}

	//这里只有当前线程自己能走到 不用先挂起
	bool bRet = true;
	{
		CAutoLock _lock(&m_csBusyThreads);
		auto it = find(m_listBusyThreads.begin(), m_listBusyThreads.end(), pThread);
		ASSERT(it != m_listBusyThreads.end());
		if (it != m_listBusyThreads.end())
		{
			m_listBusyThreads.erase(it);
		}
		else
		{
			bRet = false;
			DEL_THRD(pThread);
			return bRet;
		}
	}

	//是否多余最多空闲 多余缩减线程数
	DeleteIdleThreads();

	{
		CAutoLock _lock(&m_csIdleThreads);
		m_listIdleThreads.push_back(pThread);
	}

	//挂起自己
	if (pThread->IsRun())
	{
		pThread->SetThrdSusp();

		if (SuspendThread(pThread->GetThreadHandle()) == -1)
		{
			RetrieveErrCall(_T("EasyThrdPool：MoveBusyToIdel"));
		}
	}
	
	return bRet;
}

bool CEasyThreadPoolMgr::MoveIdleToBusy(CWorkThread* pThread, stJob* pJob)
{
	if (pThread == NULL || pJob == NULL)
	{
		assert(false);
		return false;
	}

	bool bRet = true;
	{
		CAutoLock _lock(&m_csIdleThreads);
		auto it = find(m_listIdleThreads.begin(), m_listIdleThreads.end(), pThread);
		ASSERT(it != m_listIdleThreads.end());
		if (it != m_listIdleThreads.end())
		{
			m_listIdleThreads.erase(it);
			//是否小于最小空闲 做扩充
			if (m_listIdleThreads.size() < m_nMinIdleThrdNum
				&& m_listIdleThreads.size() + m_listBusyThreads.size() < m_nMaxThrdNum)
			{
				CWorkThread* pIncreamentThrd = CreatWorkThread();
				m_listIdleThreads.push_back(pIncreamentThrd);
			}
		}
		else
		{
			ASSERT(false);
			bRet = false;
		}
	}

	if (bRet)
	{
		CAutoLock _lock(&m_csBusyThreads);
		m_listBusyThreads.push_back(pThread);
		pThread->SetJob(pJob);
	}
	
	return bRet;
}

void CEasyThreadPoolMgr::DeleteIdleThreads(int nSpinTime /* = 500 */)
{
	if (m_listIdleThreads.size() > m_nMaxIdleThrdNum)
	{
		while(nSpinTime--)
		{

		}

		//得在取size之前加锁
		CAutoLock _lock(&m_csIdleThreads);
		if (m_listIdleThreads.size() > m_nMaxIdleThrdNum)
		{
			//外面还一个等着进list
			int nCount = (m_listIdleThreads.size() + 1) / 2;
			while(nCount-- > 0)
			{
				auto it = m_listIdleThreads.begin();
				ASSERT(it != m_listIdleThreads.end());
				if (it != m_listIdleThreads.end())
				{
					//空闲里面的一定是suspend的
					DEL_THRD(*it);
					m_listIdleThreads.erase(it);
				}
			}
		}
	}
}

stJob* CEasyThreadPoolMgr::GetJob()
{
	stJob* pRet = NULL;

	CAutoLock _lock(&m_csTaskQueue);
	if (!m_TaskQueue.empty())
	{
		pRet = m_TaskQueue.top();
		m_TaskQueue.pop();
	}

	return pRet;
}

void CEasyThreadPoolMgr::ExecJob(stJob* pJob)
{
	if (pJob == NULL)
	{
		return;
	}

	//执行任务先尝试从空闲中获取
	if (!m_listIdleThreads.empty())
	{
		CWorkThread* pCurIdleThread = m_listIdleThreads.front();
		ASSERT(pCurIdleThread != NULL);
		if(!MoveIdleToBusy(pCurIdleThread, pJob))
		{
			//fall—back 理论上不永远不会走这里。。。
			ASSERT(false);
			CAutoLock _lock(&m_csTaskQueue);
			m_TaskQueue.push(pJob);
		}	
	}
	else 
	{
		//空闲中没有 尝试创建（理论一定有最小空闲数除非超了总数，但可能那个移入
		//忙的线程正在创建的未加入空闲）

		//若总数已超过最多线程数，扔去排队
		if (m_listBusyThreads.size() + m_listIdleThreads.size() >= m_nMaxThrdNum)
		{
			CAutoLock _lock(&m_csTaskQueue);
			m_TaskQueue.push(pJob);
		}
		else
		{
			CAutoLock _lock(&m_csBusyThreads);
			CWorkThread* pIncreamentThrd = CreatWorkThread();
			m_listBusyThreads.push_back(pIncreamentThrd);
			pIncreamentThrd->SetJob(pJob);
		}
	}
}

void CEasyThreadPoolMgr::TerminateJob(LPCSTR szJobKey)
{
	if (szJobKey == "")
	{
		return;
	}
	
	//排队的干掉
	{
		CAutoLock _lock(&m_csTaskQueue);
		stJob tempJob(NULL, NULL, szJobKey);
		if (m_TaskQueue.removeElem(&tempJob))
		{
			return;
		}
	}

	//正在执行的 干掉线程
	{
		CAutoLock _lock(&m_csBusyThreads);
		auto it = find_if(m_listBusyThreads.begin(), m_listBusyThreads.end(),
		[&](const CWorkThread* pThread)->bool
		{
			bool bRet = false;
			const stJob* pJob = pThread->GetJob();
			if (pJob != NULL)
			{
				bRet = pJob->strJobKey == szJobKey;
			}

			return bRet;
		});

		if (it != m_listBusyThreads.end())
		{
			CWorkThread* pThread = *it;
			::TerminateThread(pThread->GetThreadHandle(), 0);
			delete pThread;
			m_listBusyThreads.erase(it);
		}
	}
}