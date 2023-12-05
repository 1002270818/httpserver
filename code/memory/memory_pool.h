#pragma once

#include<iostream>
#include <mutex>

#define BlockSize 4096

//Slot大小即为8字节，分配就是将Slot指针交出去
struct Slot {     
    Slot* next;
};

// template <typename T, size_t BlockSize = 4096>
class MemoryPool {
public:
    MemoryPool();
    ~MemoryPool();

    void init(int size);

    // 分配或收回一个元素的内存空间
    Slot* allocate();
    void deAllocate(Slot* p);
private:
    int slotSize_;          // 每个槽所占字节数

    Slot* currentBolck_;    // 内存块链表的头指针
    Slot* currentSlot_;     // 元素链表的头指针
    Slot* lastSlot_;        // 可存放元素的最后指针
    Slot* freeSlot_;        // 元素构造后释放掉的内存链表头指针

    std::mutex mutex_freeSlot_;
    std::mutex mutex_other_;

    size_t padPointer(char* p, size_t align);   // 计算对齐所需空间
    Slot* allocateBlock();      // 申请内存块放进内存池
    Slot* nofree_solve();
};


//全局的调用内存池函数
void init_MemoryPool();
void* use_Memory(size_t size);
void free_Memory(size_t size, void* p);

// template <typename T, size_t BlockSize>
MemoryPool& get_MemoryPool(int id);

//使用内存池创建对象
template<typename T, typename... Args>
T* newElement(Args&&... args);

// 调用p的析构函数，然后将其总内存池中释放
template<typename T>
void deleteElement(T* p);