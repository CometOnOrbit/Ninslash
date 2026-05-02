#ifndef BASE_TL_MEMORY_ACCESS_H
#define BASE_TL_MEMORY_ACCESS_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace memory_opt {

// Cache line size (typically 64 bytes on modern CPUs)
static constexpr size_t CACHE_LINE_SIZE = 64;

// Prefetch distance (how far ahead to prefetch)
static constexpr size_t PREFETCH_DISTANCE = 2 * CACHE_LINE_SIZE;

// Memory access patterns
enum class AccessPattern {
    SEQUENTIAL,      // Sequential access (good for prefetch)
    RANDOM,          // Random access (hard to prefetch)
    STRIDED,         // Strided access (regular pattern)
    GATHER,          // Gather (multiple scattered reads)
    SCATTER          // Scatter (multiple scattered writes)
};

// Prefetch hints
class Prefetcher {
private:
    size_t m_PrefetchDistance;
    
public:
    explicit Prefetcher(size_t distance = PREFETCH_DISTANCE) 
        : m_PrefetchDistance(distance) {}
    
    // Prefetch for read
    template<typename T>
    void PrefetchRead(const T* address) const {
        #if defined(__GNUC__) || defined(__clang__)
            __builtin_prefetch(address, 0, 3); // 0=read, 3=high locality
        #elif defined(_MSC_VER)
            _mm_prefetch((const char*)address, _MM_HINT_T0);
        #endif
    }
    
    // Prefetch for write
    template<typename T>
    void PrefetchWrite(T* address) const {
        #if defined(__GNUC__) || defined(__clang__)
            __builtin_prefetch(address, 1, 3); // 1=write, 3=high locality
        #elif defined(_MSC_VER)
            _mm_prefetch((const char*)address, _MM_HINT_T0);
        #endif
    }
    
    // Stream prefetch (non-temporal hint)
    template<typename T>
    void PrefetchStream(const T* address) const {
        #if defined(__GNUC__) || defined(__clang__)
            __builtin_prefetch(address, 0, 0); // 0=read, 0=no locality
        #elif defined(_MSC_VER)
            _mm_prefetch((const char*)address, _MM_HINT_NTA);
        #endif
    }
    
    // Prefetch ahead in array
    template<typename T>
    void PrefetchAhead(const T* current, ptrdiff_t stride = 1) const {
        const T* ahead = current + (m_PrefetchDistance / sizeof(T)) * stride;
        PrefetchRead(ahead);
    }
    
    // Batch prefetch multiple addresses
    template<typename T, size_t N>
    void PrefetchMultiple(const T* addresses[N]) const {
        for(size_t i = 0; i < N; ++i) {
            PrefetchRead(addresses[i]);
        }
    }
};

// Non-temporal memory operations (bypass cache)
class NonTemporal {
public:
    // Non-temporal store
    template<typename T>
    static void Store(T* dest, const T& value) {
        static_assert(sizeof(T) % 16 == 0, "Non-temporal store requires 16-byte alignment");
        
        #if defined(__SSE__) || defined(__AVX__)
            if constexpr(sizeof(T) == 16) {
                _mm_stream_si128((__m128i*)dest, _mm_load_si128((const __m128i*)&value));
            } else if constexpr(sizeof(T) == 8) {
                _mm_stream_si64((long long*)dest, *(const long long*)&value);
            } else if constexpr(sizeof(T) == 4) {
                _mm_stream_si32((int*)dest, *(const int*)&value);
            } else {
                // Fallback to regular store
                *dest = value;
            }
        #else
            *dest = value;
        #endif
    }
    
    // Non-temporal copy (for large blocks)
    static void Copy(void* dest, const void* src, size_t size) {
        #if defined(__SSE__) || defined(__AVX__)
            // Use streaming stores for large copies
            if(size >= 1024) {
                size_t aligned_size = size & ~15; // 16-byte aligned
                const char* s = (const char*)src;
                char* d = (char*)dest;
                
                for(size_t i = 0; i < aligned_size; i += 16) {
                    __m128i data = _mm_loadu_si128((const __m128i*)(s + i));
                    _mm_stream_si128((__m128i*)(d + i), data);
                }
                
                // Handle remainder
                for(size_t i = aligned_size; i < size; ++i) {
                    d[i] = s[i];
                }
                return;
            }
        #endif
        
        // Fallback to memcpy
        memcpy(dest, src, size);
    }
    
    // Non-temporal set (memset alternative)
    static void Set(void* dest, int value, size_t size) {
        #if defined(__SSE__) || defined(__AVX__)
            if(size >= 1024) {
                size_t aligned_size = size & ~15;
                char* d = (char*)dest;
                __m128i pattern = _mm_set1_epi8((char)value);
                
                for(size_t i = 0; i < aligned_size; i += 16) {
                    _mm_stream_si128((__m128i*)(d + i), pattern);
                }
                
                for(size_t i = aligned_size; i < size; ++i) {
                    d[i] = (char)value;
                }
                return;
            }
        #endif
        
        memset(dest, value, size);
    }
};

// Memory streaming (for write-once data)
template<typename T>
class StreamWriter {
private:
    T* m_Current;
    T* m_End;
    Prefetcher m_Prefetcher;
    
public:
    StreamWriter(T* buffer, size_t size) 
        : m_Current(buffer), m_End(buffer + size) {}
    
    bool Write(const T& value) {
        if(m_Current >= m_End) return false;
        
        // Prefetch next write location
        m_Prefetcher.PrefetchWrite(m_Current + 1);
        
        // Non-temporal store
        NonTemporal::Store(m_Current, value);
        ++m_Current;
        
        return true;
    }
    
    template<size_t N>
    bool WriteBatch(const T (&values)[N]) {
        if(m_Current + N > m_End) return false;
        
        // Prefetch ahead
        m_Prefetcher.PrefetchWrite(m_Current + N);
        
        // Batch write
        NonTemporal::Copy(m_Current, values, N * sizeof(T));
        m_Current += N;
        
        return true;
    }
    
    size_t Written() const { return m_Current - m_Current; }
    size_t Remaining() const { return m_End - m_Current; }
};

// Cache blocking for matrix operations
template<size_t BlockSize = CACHE_LINE_SIZE>
class CacheBlocker {
public:
    // Blocked matrix multiplication
    template<typename T>
    static void MatrixMultiply(const T* A, const T* B, T* C, 
                               size_t m, size_t n, size_t p) {
        constexpr size_t block = BlockSize / sizeof(T);
        
        for(size_t i = 0; i < m; i += block) {
            size_t i_end = (i + block < m) ? i + block : m;
            
            for(size_t j = 0; j < p; j += block) {
                size_t j_end = (j + block < p) ? j + block : p;
                
                for(size_t k = 0; k < n; k += block) {
                    size_t k_end = (k + block < n) ? k + block : n;
                    
                    // Process block
                    for(size_t ii = i; ii < i_end; ++ii) {
                        for(size_t kk = k; kk < k_end; ++kk) {
                            T a = A[ii * n + kk];
                            
                            for(size_t jj = j; jj < j_end; ++jj) {
                                C[ii * p + jj] += a * B[kk * p + jj];
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Blocked array processing
    template<typename Func>
    static void ProcessArray(size_t size, Func&& func) {
        for(size_t i = 0; i < size; i += BlockSize) {
            size_t end = (i + BlockSize < size) ? i + BlockSize : size;
            
            // Process one cache line at a time
            for(size_t j = i; j < end; ++j) {
                func(j);
            }
        }
    }
};

// Memory access coalescing
template<typename T>
class AccessCoalescer {
private:
    T* m_Data;
    size_t m_Size;
    
public:
    AccessCoalescer(T* data, size_t size) : m_Data(data), m_Size(size) {}
    
    // Coalesce scattered reads into contiguous buffer
    template<typename IndicesFunc>
    void CoalesceReads(T* dest, IndicesFunc&& indices_func, size_t count) {
        // First pass: gather indices
        size_t indices[64]; // Adjust size as needed
        for(size_t i = 0; i < count && i < 64; ++i) {
            indices[i] = indices_func(i);
        }
        
        // Second pass: prefetch
        Prefetcher prefetcher;
        for(size_t i = 0; i < count && i < 64; ++i) {
            if(indices[i] < m_Size) {
                prefetcher.PrefetchRead(&m_Data[indices[i]]);
            }
        }
        
        // Third pass: actual reads
        for(size_t i = 0; i < count && i < 64; ++i) {
            if(indices[i] < m_Size) {
                dest[i] = m_Data[indices[i]];
            }
        }
    }
    
    // Coalesce scattered writes
    template<typename IndicesFunc>
    void CoalesceWrites(const T* src, IndicesFunc&& indices_func, size_t count) {
        size_t indices[64];
        for(size_t i = 0; i < count && i < 64; ++i) {
            indices[i] = indices_func(i);
        }
        
        Prefetcher prefetcher;
        for(size_t i = 0; i < count && i < 64; ++i) {
            if(indices[i] < m_Size) {
                prefetcher.PrefetchWrite(&m_Data[indices[i]]);
            }
        }
        
        for(size_t i = 0; i < count && i < 64; ++i) {
            if(indices[i] < m_Size) {
                m_Data[indices[i]] = src[i];
            }
        }
    }
};

// Write-combining buffer
template<typename T, size_t BufferSize = 64>
class WriteCombineBuffer {
private:
    T m_Buffer[BufferSize];
    size_t m_Count;
    T* m_Target;
    
public:
    WriteCombineBuffer(T* target) : m_Count(0), m_Target(target) {}
    
    void Write(const T& value) {
        m_Buffer[m_Count++] = value;
        
        if(m_Count == BufferSize) {
            Flush();
        }
    }
    
    void Flush() {
        if(m_Count > 0) {
            // Use non-temporal store for the entire buffer
            NonTemporal::Copy(m_Target, m_Buffer, m_Count * sizeof(T));
            m_Target += m_Count;
            m_Count = 0;
        }
    }
    
    ~WriteCombineBuffer() {
        Flush();
    }
};

// Memory fence/barrier utilities
class MemoryBarrier {
public:
    // Compiler barrier (prevents reordering)
    static void Compiler() {
        #if defined(__GNUC__) || defined(__clang__)
            asm volatile("" ::: "memory");
        #elif defined(_MSC_VER)
            _ReadWriteBarrier();
        #endif
    }
    
    // Full memory barrier
    static void Full() {
        #if defined(__GNUC__) || defined(__clang__)
            __sync_synchronize();
        #elif defined(_MSC_VER)
            _mm_mfence();
        #endif
    }
    
    // Read barrier
    static void Read() {
        #if defined(__GNUC__) || defined(__clang__)
            asm volatile("" ::: "memory");
        #elif defined(_MSC_VER)
            _ReadBarrier();
        #endif
    }
    
    // Write barrier
    static void Write() {
        #if defined(__GNUC__) || defined(__clang__)
            asm volatile("" ::: "memory");
        #elif defined(_MSC_VER)
            _WriteBarrier();
        #endif
    }
};

// Cache control utilities
class CacheControl {
public:
    // Flush cache line
    static void Flush(void* address) {
        #if defined(__GNUC__) || defined(__clang__)
            __builtin_ia32_clflush(address);
        #elif defined(_MSC_VER)
            _mm_clflush(address);
        #endif
    }
    
    // Prefetch to all cache levels
    static void PrefetchAll(void* address) {
        #if defined(__GNUC__) || defined(__clang__)
            __builtin_prefetch(address, 0, 3);
        #elif defined(_MSC_VER)
            _mm_prefetch((const char*)address, _MM_HINT_T0);
        #endif
    }
    
    // Prefetch with intent to write
    static void PrefetchForWrite(void* address) {
        #if defined(__GNUC__) || defined(__clang__)
            __builtin_prefetch(address, 1, 3);
        #elif defined(_MSC_VER)
            _mm_prefetch((const char*)address, _MM_HINT_T0);
        #endif
    }
};

} // namespace memory_opt

// Convenience macros
#define PREFETCH_READ(addr) memory_opt::Prefetcher().PrefetchRead(addr)
#define PREFETCH_WRITE(addr) memory_opt::Prefetcher().PrefetchWrite(addr)
#define PREFETCH_STREAM(addr) memory_opt::Prefetcher().PrefetchStream(addr)
#define PREFETCH_AHEAD(addr) memory_opt::Prefetcher().PrefetchAhead(addr)

#define NON_TEMPORAL_STORE(dest, value) memory_opt::NonTemporal::Store(dest, value)
#define NON_TEMPORAL_COPY(dest, src, size) memory_opt::NonTemporal::Copy(dest, src, size)
#define NON_TEMPORAL_SET(dest, value, size) memory_opt::NonTemporal::Set(dest, value, size)

#define MEMORY_BARRIER() memory_opt::MemoryBarrier::Full()
#define COMPILER_BARRIER() memory_opt::MemoryBarrier::Compiler()

#define CACHE_FLUSH(addr) memory_opt::CacheControl::Flush(addr)
#define CACHE_PREFETCH(addr) memory_opt::CacheControl::PrefetchAll(addr)

#endif // BASE_TL_MEMORY_ACCESS_H