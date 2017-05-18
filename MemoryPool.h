
template<typename T>
class CMemoryPool
{
public:
	CMemoryPool(int nInitSize, int nGrowSize);
	~CMemoryPool();

public:
	T* Alloc();
	void Free(T* data);

private:
	struct MemoryBlock
	{
		int nSize;
		int nFreeSize;

		T* data; //自动调整永远指向可用的
		MemoryBlock* pNestBlock;

		MemoryBlock(int _nSize);
		~MemoryBlock(){}
		T* Alloc();
		void Free(T* data);
		static void* operator new(size_t, size_t nTypeSize, size_t nSize)
		{
			return ::operator new(sizeof(MemoryBlock) + (nTypeSize > 4 ? nTypeSize : 4) * nSize);
		}

		/*static void operator delete(void* data)
		{
			::operator delete(p);
		}*/	
	};

private:
	CCriticalSection m_csMemoryBlock;
	MemoryBlock* m_pBlock;
	int m_nGrowSize;
};

struct st
{
	int a;
	char b;
	int c;
};