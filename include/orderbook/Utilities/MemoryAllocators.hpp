#pragma once
#include <memory>
#include <vector>
#include <cstddef>
#include <new>
#include <type_traits>

namespace orderbook {

/**
 * @brief Stack-based allocator for temporary objects
 * Provides very fast allocation/deallocation for short-lived objects
 */
template<size_t Size>
class StackAllocator {
public:
    static constexpr size_t STACK_SIZE = Size;
    
    StackAllocator() : current_(buffer_) {}
    
    /**
     * @brief Allocate memory from stack
     * @param size Number of bytes to allocate
     * @param alignment Memory alignment requirement
     * @return Pointer to allocated memory or nullptr if insufficient space
     */
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // Align current pointer
        auto aligned_current = align_pointer(current_, alignment);
        
        // Check if we have enough space
        if (aligned_current + size > buffer_ + STACK_SIZE) {
            return nullptr; // Out of stack space
        }
        
        current_ = aligned_current + size;
        return aligned_current;
    }
    
    /**
     * @brief Deallocate memory (no-op for stack allocator)
     * Stack allocator doesn't support individual deallocation
     */
    void deallocate(void* ptr, size_t size) {
        // No-op: stack allocator doesn't support individual deallocation
        (void)ptr;
        (void)size;
    }
    
    /**
     * @brief Reset allocator to beginning of stack
     * Effectively deallocates all allocated memory
     */
    void reset() {
        current_ = buffer_;
    }
    
    /**
     * @brief Get remaining space in stack
     * @return Number of bytes remaining
     */
    size_t remaining() const {
        return (buffer_ + STACK_SIZE) - current_;
    }
    
    /**
     * @brief Get used space in stack
     * @return Number of bytes used
     */
    size_t used() const {
        return current_ - buffer_;
    }
    
private:
    alignas(std::max_align_t) char buffer_[STACK_SIZE];
    char* current_;
    
    /**
     * @brief Align pointer to specified alignment
     */
    static char* align_pointer(char* ptr, size_t alignment) {
        auto addr = reinterpret_cast<uintptr_t>(ptr);
        auto aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
        return reinterpret_cast<char*>(aligned_addr);
    }
};

/**
 * @brief STL-compatible allocator using StackAllocator
 */
template<typename T, size_t StackSize = 64 * 1024> // 64KB default
class StackSTLAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = StackSTLAllocator<U, StackSize>;
    };
    
    StackSTLAllocator() = default;
    
    template<typename U>
    StackSTLAllocator(const StackSTLAllocator<U, StackSize>&) {}
    
    pointer allocate(size_type n) {
        if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }
        
        void* ptr = stack_allocator_.allocate(n * sizeof(T), alignof(T));
        if (!ptr) {
            // Fall back to heap allocation if stack is full
            ptr = std::malloc(n * sizeof(T));
            if (!ptr) {
                throw std::bad_alloc();
            }
            heap_allocated_.push_back(ptr);
        }
        
        return static_cast<pointer>(ptr);
    }
    
    void deallocate(pointer ptr, size_type n) {
        // Check if this was heap allocated
        auto it = std::find(heap_allocated_.begin(), heap_allocated_.end(), ptr);
        if (it != heap_allocated_.end()) {
            std::free(ptr);
            heap_allocated_.erase(it);
        }
        // Stack allocated memory is deallocated when allocator is reset
    }
    
    template<typename U, typename... Args>
    void construct(U* ptr, Args&&... args) {
        new(ptr) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* ptr) {
        ptr->~U();
    }
    
    /**
     * @brief Reset stack allocator
     */
    void reset() {
        stack_allocator_.reset();
        // Clean up any heap allocations
        for (void* ptr : heap_allocated_) {
            std::free(ptr);
        }
        heap_allocated_.clear();
    }
    
    /**
     * @brief Get allocator statistics
     */
    struct Stats {
        size_t stack_used;
        size_t stack_remaining;
        size_t heap_allocations;
    };
    
    Stats getStats() const {
        return {
            stack_allocator_.used(),
            stack_allocator_.remaining(),
            heap_allocated_.size()
        };
    }
    
private:
    static thread_local StackAllocator<StackSize> stack_allocator_;
    static thread_local std::vector<void*> heap_allocated_;
};

// Thread-local storage for stack allocator
template<typename T, size_t StackSize>
thread_local StackAllocator<StackSize> StackSTLAllocator<T, StackSize>::stack_allocator_;

template<typename T, size_t StackSize>
thread_local std::vector<void*> StackSTLAllocator<T, StackSize>::heap_allocated_;

/**
 * @brief Memory pool allocator for fixed-size objects
 * Optimized for frequent allocation/deallocation of same-sized objects
 */
template<typename T, size_t BlockSize = 1024>
class PoolAllocator {
public:
    static constexpr size_t BLOCK_SIZE = BlockSize;
    static constexpr size_t OBJECTS_PER_BLOCK = BLOCK_SIZE / sizeof(T);
    
    struct Block {
        alignas(T) char data[BLOCK_SIZE];
        Block* next = nullptr;
    };
    
    PoolAllocator() {
        allocateNewBlock();
    }
    
    ~PoolAllocator() {
        while (blocks_) {
            Block* next = blocks_->next;
            delete blocks_;
            blocks_ = next;
        }
    }
    
    T* allocate() {
        if (!free_list_) {
            allocateNewBlock();
        }
        
        T* result = free_list_;
        free_list_ = *reinterpret_cast<T**>(free_list_);
        return result;
    }
    
    void deallocate(T* ptr) {
        if (!ptr) return;
        
        *reinterpret_cast<T**>(ptr) = free_list_;
        free_list_ = ptr;
    }
    
    /**
     * @brief Get allocator statistics
     */
    struct Stats {
        size_t blocks_allocated;
        size_t objects_per_block;
        size_t total_capacity;
    };
    
    Stats getStats() const {
        size_t block_count = 0;
        Block* current = blocks_;
        while (current) {
            ++block_count;
            current = current->next;
        }
        
        return {
            block_count,
            OBJECTS_PER_BLOCK,
            block_count * OBJECTS_PER_BLOCK
        };
    }
    
private:
    void allocateNewBlock() {
        Block* new_block = new Block();
        new_block->next = blocks_;
        blocks_ = new_block;
        
        // Initialize free list for this block
        char* start = new_block->data;
        char* end = start + BLOCK_SIZE;
        
        for (char* ptr = start; ptr + sizeof(T) <= end; ptr += sizeof(T)) {
            T* obj = reinterpret_cast<T*>(ptr);
            *reinterpret_cast<T**>(obj) = free_list_;
            free_list_ = obj;
        }
    }
    
    Block* blocks_ = nullptr;
    T* free_list_ = nullptr;
};

/**
 * @brief SIMD-aligned allocator for vectorized operations
 */
template<typename T, size_t Alignment = 32> // 32-byte alignment for AVX
class AlignedAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    static constexpr size_t alignment = Alignment;
    
    template<typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };
    
    AlignedAllocator() = default;
    
    template<typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) {}
    
    pointer allocate(size_type n) {
        if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::bad_alloc();
        }
        
        size_t size = n * sizeof(T);
        void* ptr = std::aligned_alloc(alignment, size);
        if (!ptr) {
            throw std::bad_alloc();
        }
        
        return static_cast<pointer>(ptr);
    }
    
    void deallocate(pointer ptr, size_type n) {
        std::free(ptr);
    }
    
    template<typename U, typename... Args>
    void construct(U* ptr, Args&&... args) {
        new(ptr) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* ptr) {
        ptr->~U();
    }
    
    bool operator==(const AlignedAllocator&) const { return true; }
    bool operator!=(const AlignedAllocator&) const { return false; }
};

// Convenient type aliases
template<typename T>
using StackVector = std::vector<T, StackSTLAllocator<T>>;

template<typename T>
using AlignedVector = std::vector<T, AlignedAllocator<T>>;

} // namespace orderbook