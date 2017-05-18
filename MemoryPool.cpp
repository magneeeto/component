#include "stdafx.h"
#include "AutoLock.h"
#include "MemoryPool.h"

template<typename T>
CMemoryPool<T>::MemoryBlock::MemoryBlock(int _nSize)
	: nSize(_nSize), nFreeSize(_nSize), pNestBlock(NULL)
{
	data = (T*)((char*)this + sizeof(MemoryBlock));

	//每块空闲连接起来
	for (int nIndex = 0; nIndex < nSize - 1; nIndex++)
	{
		*(int*)(&data[nIndex]) =(int)(&data[nIndex + 1]);
	}

	*(int*)(&data[nSize - 1]) = NULL;
}

template<typename T>
T* CMemoryPool<T>::MemoryBlock::Alloc()
{
	//外面控制理论上FreeSize大于0
	if (nFreeSize == 0)
	{
		assert(false);
		return NULL;
	}

	T* pRet = data;
	data = (T*)(*((int*)data));
	nFreeSize--;

	return pRet;
}

template<typename T>
void CMemoryPool<T>::MemoryBlock::Free(T* _data)
{
	//理论上data的地址一定在改block范围内
	if ((ULONG)_data < (ULONG)this || (ULONG)_data > (ULONG)this + sizeof(MemoryBlock) + (sizeof(T) > 4 ? sizeof(T) : 4) * nSize)
	{
		assert(false);
		return;
	}

	*((int*)_data) = (int)data;
	data = _data;
	nFreeSize++;
}

template<typename T>
CMemoryPool<T>::CMemoryPool(int nInitSize, int nGrowSize)
	: m_nGrowSize(nGrowSize)
{
	m_pBlock = new(sizeof(T), nInitSize) MemoryBlock(nInitSize);
	m_pBlock->pNestBlock = NULL;
}
template<typename T>
CMemoryPool<T>::~CMemoryPool()
{
	while(m_pBlock)
	{
		MemoryBlock* pHelper = m_pBlock;
		m_pBlock = m_pBlock->pNestBlock;
		delete pHelper;
	}
}

template<typename T>
T* CMemoryPool<T>::Alloc()
{
	CAutoLock _lock(&m_csMemoryBlock);
	MemoryBlock* pAllocBlock = m_pBlock;
	while(pAllocBlock != NULL && pAllocBlock->nFreeSize <= 0)
		pAllocBlock = pAllocBlock->pNestBlock;

	if (pAllocBlock == NULL)
	{
		pAllocBlock = new(sizeof(T), m_nGrowSize) MemoryBlock(m_nGrowSize);
		pAllocBlock->pNestBlock = m_pBlock;
		m_pBlock = pAllocBlock;	
	}
	
	return pAllocBlock->Alloc();
}

template<typename T>
void CMemoryPool<T>::Free(T* data)
{
	CAutoLock _lock(&m_csMemoryBlock);
	MemoryBlock* pHelper = m_pBlock;
	while(pHelper != NULL)
	{
		if ((ULONG)pHelper < (ULONG)data && (ULONG)data < (ULONG)pHelper + sizeof(MemoryBlock) + (sizeof(T) > 4 ? sizeof(T) : 4) * pHelper->nSize)
		{
			pHelper->Free(data);
			break;
		}
		pHelper = pHelper->pNestBlock;
	}

	if (pHelper->nFreeSize > pHelper->nSize / 2)
	{
		MemoryBlock* pFront = NULL;
		MemoryBlock* pTemp = m_pBlock;
		while(pTemp != NULL)
		{
			if (pTemp->nFreeSize == pTemp->nSize)
			{
				pTemp == m_pBlock ? m_pBlock = pTemp->pNestBlock
					: pFront->pNestBlock = pTemp->pNestBlock;
				delete(pTemp);
				break;
			}
			pFront = pTemp;
			pTemp = pTemp->pNestBlock;
		}
	}
}

template class CMemoryPool<st>;