#ifndef UM_COMMON_AUTOLOCK_H_
#define UM_COMMON_AUTOLOCK_H_

namespace UMLink
{
	class CCriticalSection;
	class CMutex;
	
	template<typename T>
	class CAutoLock
	{
	public:
		CAutoLock(T* pLock)
		{
			m_pLock = pLock;
			m_pLock->Lock();
		}

		~CAutoLock()
		{
			m_pLock->Unlock();
		}

	private:
		T* m_pLock;
	};

	typedef CAutoLock<CMutex> CAutoMutex;
	typedef CAutoLock<CCriticalSection> CAutoCS;

	class CCriticalSection
	{
	public:
		CCriticalSection()
		{
			InitializeCriticalSection(&m_cs);
		}

		~CCriticalSection()
		{
			DeleteCriticalSection(&m_cs);
		}

		void Lock()
		{
			EnterCriticalSection(&m_cs);
		}
		void Unlock()
		{
			LeaveCriticalSection(&m_cs);
		}

	private:
		CCriticalSection(const CCriticalSection&);
		CCriticalSection& operator=(const CCriticalSection&);
	private:
		CRITICAL_SECTION m_cs;
	};

	class CMutex
	{
	public:
		CMutex(const char* szName)
			: m_hMutex(NULL), m_dErrorCode(NO_ERROR)
		{
			m_hMutex = CreateMutexA(NULL, false, szName);
			m_dErrorCode = GetLastError();
			if (m_hMutex == NULL)
			{
				assert(false);
			}
		}

		~CMutex()
		{
			if (m_hMutex)
			{
				CloseHandle(m_hMutex);
				m_hMutex = NULL;
			}
		}

		void Lock()
		{
			WaitForSingleObject(m_hMutex, INFINITE);
		}

		void Unlock()
		{
			ReleaseMutex(m_hMutex);
		}

		bool IsValid()
		{
			return m_hMutex != NULL;
		}

	private:
		CMutex(const CMutex&);
		CMutex& operator=(const CMutex&);
	private:
		HANDLE m_hMutex;
		DWORD m_dErrorCode;
	};

	class CSemaphore
	{
	public:
		CSemaphore(const char* szName, long lInitNum, long lMaxNum)
			: m_hSemaphore(NULL), m_dErrorCode(NO_ERROR)
		{
			m_hSemaphore = CreateSemaphoreA(NULL, lInitNum, lMaxNum, szName);
			m_dErrorCode = GetLastError();
			if (m_hSemaphore == NULL)
			{				
				assert(false);
			}
		}

		~CSemaphore()
		{
			if (m_hSemaphore)
			{
				CloseHandle(m_hSemaphore);
				m_hSemaphore = NULL;
			}
		}

		void Lock()
		{
			WaitForSingleObject(m_hSemaphore, INFINITE);
		}

		void Unlock()
		{
			ReleaseSemaphore(m_hSemaphore, 1, NULL);
		}

		bool IsValid()
		{
			return m_hSemaphore != NULL;
		}

	private:
		CSemaphore(const CSemaphore&);
		CSemaphore& operator=(const CSemaphore&);

	private:
		HANDLE m_hSemaphore;
		DWORD m_dErrorCode;
	};
}

#endif //UM_COMMON_AUTOLOCK_H_