#include "stdafx.h"
#include "SharedMemory.h"

const char* szMutexTail = "%_Mutex";
const char* szEmptyTail = "%_EmptySmph";
const char* szFullTail = "%_FullSmph";

namespace UMLink
{
	CSharedMemory::CSharedMemory()
		: m_mutex(NULL), m_hFileMap(NULL), m_pMapView(NULL)
		, m_emptySmph(NULL), m_fullSmph(NULL), m_nSize(0), m_bIsValid(false)
	{

	}

	CSharedMemory::~CSharedMemory()
	{
		CAutoMutex lock(m_mutex.get());

		UnmapViewOfFile(m_pMapView);
		CloseHandle(m_hFileMap);
	}

	CSharedMemory::SharedMemoryError CSharedMemory::InitLock(const char* name)
	{
		SharedMemoryError ret = SME_NOERROR;
		
		do 
		{
			std::string strMutexName(name);
			strMutexName.append(szMutexTail);
			m_mutex = std::make_shared<CMutex>(strMutexName.c_str());
			if (!m_mutex->IsValid())
			{
				ret = SME_CREATEMUTEXFAILED;
				break;
			}

			std::string strEmptySmphName(name);
			strEmptySmphName.append(szEmptyTail);
			m_emptySmph = std::make_shared<CSemaphore>(strEmptySmphName.c_str(), 1, 1);
			if (!m_emptySmph->IsValid())
			{
				ret = SME_CREATEEMPTYSMPHFAILED;
				break;
			}

			std::string strFullSmphName(name);
			strFullSmphName.append(szFullTail);
			m_fullSmph = std::make_shared<CSemaphore>(strFullSmphName.c_str(), 0, 1);
			if (!m_fullSmph->IsValid())
			{
				ret = SME_CREATEFULLSMPHFAILED;
				break;
			}
		} while (false);

		return ret;
	}

	CSharedMemory::SharedMemoryError CSharedMemory::InitSharedMemroy(const char* name, unsigned int size)
	{
		SharedMemoryError ret = SME_NOERROR;
		m_nSize = size;
		do 
		{
			if ((ret = InitLock(name)) != SME_NOERROR)
			{
				LD("ShareMemory::InitLock Error::" << ret);
				break;
			}

			CAutoMutex lock(m_mutex.get());
			bool bExist = false;
			m_hFileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, name);
			DWORD errorCode = GetLastError();
			if (m_hFileMap == NULL)
			{
				assert(false);
				LD("ShareMemory::CreateFileMapping Error:" << errorCode);
				ret = SME_CREATEFILEMAPINGFAILED;	
				break;
			}
			else
			{
				if (errorCode == 183)
				{
					bExist = true;
				}
			}

			m_pMapView = MapViewOfFile(m_hFileMap, FILE_MAP_READ | FILE_MAP_WRITE, 0 ,0 ,0);
			DWORD errorCode2 = GetLastError();
			if (m_pMapView == NULL)
			{
				assert(false);
				LD("ShareMemory::MapViewOfFile Error:" << errorCode2);
				ret = SME_MAPVIEWOFFILEFAILED;
				break;
			}
			
			bExist ? void() : memset(m_pMapView, 0, size);
			m_bIsValid = true;

		} while (false);

		return ret;
	}

	bool CSharedMemory::ReadData(MsgType& type, void** data)
	{
		LD("SharedMemory::ReadData::Enter");
		bool bRet = false;

		if (m_pMapView != NULL)
		{
			LD("SharedMemory::ReadData::MapViewValid");
			bRet = true;

			m_fullSmph->Lock();
			LD("SharedMemory::ReadData::GetFullSemaphore");
			//目前仅一个资源不用加锁 可以考虑指定容器allocator从mapView中申请内存实现多资源
			//CAutoMutex _lock(m_mutex.get());
			memcpy((unsigned char*)&type, m_pMapView, UMWEB_SHAREDMEMORY_MSGTYPESIZE);
			if (type == UMWEB_SHAREDMEMORY_QUITMSG)
			{
				LD("SharedMemory::ReadData::GetQuitMessage");
				*data = NULL;
				m_emptySmph->Unlock();
				return false;
			}
			
			unsigned int size = m_nSize - UMWEB_SHAREDMEMORY_MSGTYPESIZE;
			*data = malloc(size);
			memset(*data, 0, size);
			memcpy(*data, (unsigned char*)m_pMapView + UMWEB_SHAREDMEMORY_MSGTYPESIZE, size);

			//clear
			memset(m_pMapView, 0, m_nSize);
			m_emptySmph->Unlock();
			LD("SharedMemory::ReadData::ReleaseEmptySemaphore");
		}

		LD("SharedMemory::ReadData::Return=" << bRet);
		return bRet;
	}

	void CSharedMemory::WriteData(MsgType type, const void* data, int size)
	{
		LD("SharedMemory::WriteData::Enter::Type=" << type);
		if (m_pMapView != NULL)
		{
			m_emptySmph->Lock();
			LD("SharedMemory::WriteData::GetEmptySemaphore");

			//目前仅一个资源不用加锁 可以考虑指定容器allocator从mapView中申请内存实现多资源
			//CAutoMutex _lock(m_mutex.get());
			memcpy(m_pMapView, (unsigned char*)&type, UMWEB_SHAREDMEMORY_MSGTYPESIZE);
			if(size > 0)
				memcpy((unsigned char*)m_pMapView + UMWEB_SHAREDMEMORY_MSGTYPESIZE, (unsigned char*)data, size);
			//*(static_cast<T*>((void*)((unsigned char*)m_pMapView + UMWEB_SHAREDMEMORY_MSGTYPESIZE))) = data;
			
			m_fullSmph->Unlock();
			LD("SharedMemory::WriteData::ReleaseFullSemaphore");
		}
	}
}
