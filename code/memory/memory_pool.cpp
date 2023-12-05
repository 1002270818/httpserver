#include "memory_pool.h"

MemoryPool::MemoryPool() {}

MemoryPool::~MemoryPool() {
    Slot* cur = currentBolck_;
    while(cur) {
        Slot* next = cur->next;
        // free(reinterpret_cast<void *>(cur));
        // 转化为 void 指针，是因为 void 类型不需要调用析构函数,只释放空间
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

void MemoryPool::init(int size) {
    slotSize_ = size;
    currentBolck_ = NULL;
    currentSlot_ = NULL;
    lastSlot_ = NULL;
    freeSlot_ = NULL;
}

// 计算对齐所需补的空间
inline size_t MemoryPool::padPointer(char* p, size_t align) {
    size_t result = reinterpret_cast<size_t>(p);
    return ((align - result) % align);
}

Slot* MemoryPool::allocateBlock() {
    char* newBlock = reinterpret_cast<char *>(operator new(BlockSize));     //重解转换：char数组指针

    char* body = newBlock + sizeof(Slot*);
    // 计算为了对齐需要空出多少位置
    size_t bodyPadding = padPointer(body, static_cast<size_t>(slotSize_));
    
    // 注意：多个线程共用一个MemoryPool
    Slot* useSlot;
    {
        std::lock_guard<std::mutex> lock(mutex_other_);
        // newBlock接到Block链表的头部
        reinterpret_cast<Slot *>(newBlock)->next = currentBolck_;
        currentBolck_ = reinterpret_cast<Slot *>(newBlock);
        // 为该Block开始的地方加上bodyPadding个char* 空间
        currentSlot_ = reinterpret_cast<Slot *>(body + bodyPadding);
        lastSlot_ = reinterpret_cast<Slot *>(newBlock + BlockSize - slotSize_ + 1);
        useSlot = currentSlot_;

        // slot指针一次移动8个字节
        currentSlot_ += (slotSize_ >> 3);
        // currentSlot_ += slotSize_;
    }

    return useSlot;
}

Slot* MemoryPool::nofree_solve() {
    if(currentSlot_ >= lastSlot_)
        return allocateBlock();
    Slot* useSlot;
    {
        std::lock_guard<std::mutex> lock(mutex_other_);
        useSlot = currentSlot_;
        // slot指针一次移动8个字节(指针值加减)
        currentSlot_ += (slotSize_ >> 3);
    }

    return useSlot;
}

Slot* MemoryPool::allocate() {
    if(freeSlot_) {
        {
            std::lock_guard<std::mutex> lock(mutex_freeSlot_);
            if(freeSlot_) {
                Slot* result = freeSlot_;
                freeSlot_ = freeSlot_->next;
                return result;
            }
        }
    }

    return nofree_solve();
}

inline void MemoryPool::deAllocate(Slot* p) {
    if(p) {
        // 将slot加入释放队列
        std::lock_guard<std::mutex> lock(mutex_freeSlot_);
        p->next = freeSlot_;
        freeSlot_ = p;
    }
}

// template <typename T, size_t BlockSize>
MemoryPool& get_MemoryPool(int id) {
    static MemoryPool memorypool_[65];
    return memorypool_[id];
}

// 数组中分别存放Slot大小为8，16，...，1024字节的BLock链表
void init_MemoryPool() {
    for(int i = 0; i < 65; ++i) {
        get_MemoryPool(i).init((i + 1) << 3);
    }
}

// 超过1024字节就直接new
void* use_Memory(size_t size) {
    if(!size)
        return nullptr;
    if(size > 1024)
        return operator new(size);
    
    // 相当于(size / 8)向上取整:小于8 id为0,小于16 id为1,小于24 id为2...以此类推
    return reinterpret_cast<void *>(get_MemoryPool(((size + 7) >> 3) - 1).allocate());
}

void free_Memory(size_t size, void* p) {
    if(!p)  return;
    if(size > 1024) {
        operator delete (p);
        return;
    }
    get_MemoryPool(((size + 7) >> 3) - 1).deAllocate(reinterpret_cast<Slot *>(p));
}

template<typename T, typename... Args>
T* newElement(Args&&... args) {
    T* p;
    if((p = reinterpret_cast<T *>(use_Memory(sizeof(T)))) != nullptr)
        // new(p) T1(value);
        // placement new:在指针p所指向的内存空间创建一个T1类型的对象，类似与 realloc
        // 把已有的空间当成一个缓冲区来使用，减少了分配空间所耗费的时间
        // 因为直接用new操作符分配内存的话，在堆中查找足够大的剩余空间速度是比较慢的
        new(p) T(std::forward<Args>(args)...); // 完美转发

    return p;
}

template<typename T>
void deleteElement(T* p) {
    // printf("deleteElement...\n");
    if(p)
        p->~T();
    free_Memory(sizeof(T), reinterpret_cast<void *>(p));
    // printf("deleteElement success\n");
}