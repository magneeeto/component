#include "StdAfx.h"
#include ".\taskqueuethread.h"
#include <process.h>
#include <tchar.h>

int CTaskQueueThread::m_nID = 0;

CTaskQueueThread::CTaskQueueThread(LPCSTR szThreadName/* = nullptr*/)
	:m_bSingleTask(false), m_hThread(NULL), m_bExit(false)
{
	CString strEventName = _T("CTaskQueueThread_Event_");
	strEventName.Append(CI2W(GetCurrentProcessId()));
	strEventName.Append(_T("_"));
	strEventName.Append(CI2W(m_nID++));
	m_hEvent = CreateEvent(NULL, TRUE, TRUE,  strEventName);

	if (szThreadName != nullptr)
	{
		m_strThreadName = szThreadName;
	}
}

CTaskQueueThread::CTaskQueueThread(FNCallBack *pFunc, void* param
	, bool bSingleTask/* = true*/, LPCSTR szThreadName/* = nullptr*/):m_bExit(false)
	,m_bSingleTask(bSingleTask),m_hThread(NULL)
{
	PushTask(pFunc, param);
	CString strEventName = _T("CTaskQueueThread_Event_");
	strEventName.Append(CI2W(GetCurrentProcessId()));
	strEventName.Append(_T("_"));
	strEventName.Append(CI2W(m_nID++));
	m_hEvent = CreateEvent(NULL, TRUE, TRUE,  strEventName);
	SetEvent(m_hEvent);

	if (szThreadName != nullptr)
	{
		m_strThreadName = szThreadName;
	}
}

CTaskQueueThread::~CTaskQueueThread(void)
{
	if (!m_bSingleTask)
	{
		m_bExit = true;
		::SetEvent(m_hEvent);
	}
	if (m_hThread != nullptr)
	{
		CloseHandle(m_hThread);
		m_hThread = nullptr;
	}
}

BOOL CTaskQueueThread::PushTask(FNCallBack *pFunc, void* param)
{
	stTask task;
	task.pFunc = pFunc;
	task.param = param;

	CAutoLock _lock(&m_csQueueFnCallBack);
	m_queueFnCallBack.push(task);
	
	if (!m_bSingleTask)
	{
		::SetEvent(m_hEvent);
	}
	return m_queueFnCallBack.size()==1;
}

bool CTaskQueueThread::ClearTask(bool bExit/* = true*/)
{
	LD("enter");
	CAutoLock _lock(&m_csQueueFnCallBack);
	while(!m_queueFnCallBack.empty())
	{
		m_queueFnCallBack.pop();
	}
	LD("exit "<<m_queueFnCallBack.size());
	m_bExit = bExit;
	::SetEvent(m_hEvent);
	return true;
}

BOOL CTaskQueueThread::Start()
{
	if(isRunning())
	{
		return FALSE;
	}

	if(NULL != m_hThread)
	{
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}

	unsigned int nThreadID;
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, Run, this, 1, &nThreadID);
	
	if (!m_strThreadName.empty())
	{
		SetThreadName(nThreadID, m_strThreadName.c_str());
	}
	return TRUE;
}

void CTaskQueueThread::Terminate()
{
	if(NULL == m_hThread)
	{
		return;
	}

	::TerminateThread(m_hThread, 0);
	CloseHandle(m_hThread);
	m_hThread = NULL;	
}

HANDLE CTaskQueueThread::GetThreadHandle()
{
	return m_hThread;
}

void CTaskQueueThread::ExecuteTasks()
{
	bool bEmpty = false;
	{
		CAutoLock _lock(&m_csQueueFnCallBack);
		bEmpty = m_queueFnCallBack.empty();
	}
	while (!bEmpty)
	{
		if (m_bExit)
		{
			break;
		}
		stTask task;
		{
			CAutoLock _lock(&m_csQueueFnCallBack);
			task = m_queueFnCallBack.front();
			m_queueFnCallBack.pop();
			bEmpty = m_queueFnCallBack.empty();
		}
		(*(task.pFunc))(task.param);
	}
}

unsigned int __stdcall CTaskQueueThread::Run(void *param)
{
	CTaskQueueThread *pThis = (CTaskQueueThread *)param;
	if (pThis == NULL)
	{
		return 0;
	}
	
	if (pThis->m_bSingleTask)
	{
		pThis->ExecuteTasks();
	}
	else
	{
		while(true)
		{
			DWORD dwRet = WaitForSingleObject(pThis->m_hEvent, INFINITE);
			if (WAIT_FAILED == dwRet)
			{
				break;
			}
			if (pThis->m_bExit)
			{
				break;
			}
			ResetEvent(pThis->m_hEvent);
			pThis->ExecuteTasks();
		}
		//m_hEvent可能在别的线程释放了
		//CloseHandle(pThis->m_hEvent);
		//pThis->m_hEvent = nullptr;
	}

	return 0;
}

bool CTaskQueueThread::isRunning()
{
	if(NULL == m_hThread)
	{
		return false;
	}

	DWORD ec = 0;
	return GetExitCodeThread(m_hThread, &ec) && ec == STILL_ACTIVE;
}

void CTaskQueueThread::Wait()
{
	if(NULL == m_hThread)
	{
		return;
	}

	switch (WaitForSingleObject(m_hThread, INFINITE))
	{
	case WAIT_OBJECT_0:
		{
			CloseHandle(m_hThread);
			m_hThread = NULL;
			return;
		}
	default:
		break;
	}
}

