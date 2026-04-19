#ifndef BASE_TL_OBJECT_POOL_H
#define BASE_TL_OBJECT_POOL_H

#include <vector>
#include <cstddef>

template<typename T, size_t PoolSize = 256>
class CObjectPool
{
private:
    union PoolItem
    {
        T m_Object;
        PoolItem* m_pNextFree;
        
        PoolItem() {}
        ~PoolItem() {}
    };
    
    PoolItem m_aItems[PoolSize];
    PoolItem* m_pFirstFree;
    std::vector<T*> m_ActiveObjects;
    
public:
    CObjectPool()
    {
        // Initialize free list
        for(size_t i = 0; i < PoolSize - 1; ++i)
        {
            m_aItems[i].m_pNextFree = &m_aItems[i + 1];
        }
        m_aItems[PoolSize - 1].m_pNextFree = nullptr;
        m_pFirstFree = &m_aItems[0];
        
        m_ActiveObjects.reserve(PoolSize);
    }
    
    ~CObjectPool()
    {
        // Destroy all active objects
        for(T* pObj : m_ActiveObjects)
        {
            pObj->~T();
        }
    }
    
    template<typename... Args>
    T* Allocate(Args&&... args)
    {
        if(!m_pFirstFree)
        {
            // Pool exhausted, fall back to new
            T* pObj = new T(std::forward<Args>(args)...);
            m_ActiveObjects.push_back(pObj);
            return pObj;
        }
        
        // Get item from pool
        PoolItem* pItem = m_pFirstFree;
        m_pFirstFree = pItem->m_pNextFree;
        
        // Construct object in-place
        T* pObj = new (&pItem->m_Object) T(std::forward<Args>(args)...);
        m_ActiveObjects.push_back(pObj);
        
        return pObj;
    }
    
    void Deallocate(T* pObj)
    {
        // Find and remove from active objects
        for(auto it = m_ActiveObjects.begin(); it != m_ActiveObjects.end(); ++it)
        {
            if(*it == pObj)
            {
                m_ActiveObjects.erase(it);
                break;
            }
        }
        
        // Check if object is from our pool
        PoolItem* pItem = reinterpret_cast<PoolItem*>(pObj);
        bool fromPool = false;
        
        for(size_t i = 0; i < PoolSize; ++i)
        {
            if(&m_aItems[i] == pItem)
            {
                fromPool = true;
                break;
            }
        }
        
        if(fromPool)
        {
            // Destroy object and add back to free list
            pObj->~T();
            pItem->m_pNextFree = m_pFirstFree;
            m_pFirstFree = pItem;
        }
        else
        {
            // Object was allocated with new
            delete pObj;
        }
    }
    
    size_t GetActiveCount() const { return m_ActiveObjects.size(); }
    size_t GetFreeCount() const 
    { 
        size_t count = 0;
        PoolItem* pItem = m_pFirstFree;
        while(pItem)
        {
            ++count;
            pItem = pItem->m_pNextFree;
        }
        return count;
    }
    size_t GetTotalCount() const { return PoolSize; }
    
    void Reset()
    {
        // Destroy all active objects
        for(T* pObj : m_ActiveObjects)
        {
            // Check if from pool
            PoolItem* pItem = reinterpret_cast<PoolItem*>(pObj);
            bool fromPool = false;
            
            for(size_t i = 0; i < PoolSize; ++i)
            {
                if(&m_aItems[i] == pItem)
                {
                    fromPool = true;
                    break;
                }
            }
            
            if(fromPool)
            {
                pObj->~T();
                // Don't add back to free list here, will be reinitialized
            }
            else
            {
                delete pObj;
            }
        }
        
        m_ActiveObjects.clear();
        
        // Reinitialize free list
        for(size_t i = 0; i < PoolSize - 1; ++i)
        {
            m_aItems[i].m_pNextFree = &m_aItems[i + 1];
        }
        m_aItems[PoolSize - 1].m_pNextFree = nullptr;
        m_pFirstFree = &m_aItems[0];
    }
};

#endif // BASE_TL_OBJECT_POOL_H