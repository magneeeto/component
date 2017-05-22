#ifndef UM_COMMON_SHAREDMEMORY_H_
#define UM_COMMON_SHAREDMEMORY_H_

#include "AutoLock.h"

#define UMWEB_SHAREDMEMORY_NAME "UMLINK_SHAREMEMORY_DD6D1FC3-A8E1-4D26-9B10-9775FC9D5C11"
#define UMWEB_SHAREDMEMORY_SIZE 2048

namespace UMLink
{
	class CSharedMemory 
	{
	public:
		typedef enum emSharedMemoryError
		{
			SME_NOERROR,
			SME_CREATEMUTEXFAILED,
			SME_CREATEFILEMAPINGFAILED,
			SME_MAPVIEWOFFILEFAILED,
			SME_CREATEFULLSMPHFAILED,
			SME_CREATEEMPTYSMPHFAILED,

		} SharedMemoryError;

		typedef unsigned int MsgType;
		#define UMWEB_SHAREDMEMORY_MSGTYPESIZE sizeof(MsgType)
		#define UMWEB_SHAREDMEMORY_QUITMSG 0x0000

	public:
		CSharedMemory();
		~CSharedMemory();

	public:
		SharedMemoryError InitSharedMemroy(const char* name, unsigned int size);
		
		bool IsValid()
		{
			return m_bIsValid;
		}

		//需要管理内存
		bool ReadData(MsgType& type, void** data);
		
		void WriteData(MsgType type, const void* data, int size);
		
		template<typename T>
		void WriteData(MsgType type, const T& data)
		{
			if (m_pMapView != NULL)
			{
				WriteData(type, &data, sizeof(T));
			}
		}

	private:
		SharedMemoryError InitLock(const char* name);

	protected:
		bool m_bIsValid;
		HANDLE m_hFileMap;
		LPVOID m_pMapView;
		unsigned int m_nSize;
		std::shared_ptr<CMutex> m_mutex;
		std::shared_ptr<CSemaphore> m_emptySmph;
		std::shared_ptr<CSemaphore> m_fullSmph;
	};
}

#endif //UM_COMMON_SHAREDMEMORY_H_