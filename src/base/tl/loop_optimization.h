#ifndef BASE_TL_LOOP_OPTIMIZATION_H
#define BASE_TL_LOOP_OPTIMIZATION_H

#include <cstddef>
#include <type_traits>
#include <cstring>

namespace loop_opt {

// Loop unrolling templates
template<size_t N>
struct Unroll {
    template<typename Func>
    static void Execute(Func&& func) {
        Unroll<N-1>::Execute(std::forward<Func>(func));
        func(N-1);
    }
};

template<>
struct Unroll<0> {
    template<typename Func>
    static void Execute(Func&&) {}
};

// Loop unroll macro for known iteration counts
#define UNROLL_LOOP(n, func) loop_opt::Unroll<n>::Execute([&](size_t i) { func(i); })

// Loop unroll with partial unrolling for runtime counts
template<size_t UnrollFactor = 4>
class PartialUnroll {
public:
    template<typename Func>
    static void Execute(size_t count, Func&& func) {
        size_t i = 0;
        
        // Main unrolled loop
        for(; i + UnrollFactor <= count; i += UnrollFactor) {
            func(i);
            func(i + 1);
            func(i + 2);
            func(i + 3);
        }
        
        // Remainder loop
        for(; i < count; ++i) {
            func(i);
        }
    }
    
    template<typename Func>
    static void ExecuteWithIndex(size_t count, Func&& func) {
        size_t i = 0;
        
        for(; i + UnrollFactor <= count; i += UnrollFactor) {
            func(i, i);
            func(i + 1, i + 1);
            func(i + 2, i + 2);
            func(i + 3, i + 3);
        }
        
        for(; i < count; ++i) {
            func(i, i);
        }
    }
};

// Vectorization hints
#if defined(__GNUC__) || defined(__clang__)
    #define VECTORIZE_LOOP _Pragma("GCC ivdep")
    #define VECTORIZE_ALWAYS _Pragma("GCC ivdep")
    #define NO_VECTORIZE _Pragma("GCC novector")
    #define UNROLL_LOOP_HINT(n) _Pragma("GCC unroll " #n)
#elif defined(_MSC_VER)
    #define VECTORIZE_LOOP __pragma(loop(ivdep))
    #define VECTORIZE_ALWAYS __pragma(loop(ivdep))
    #define NO_VECTORIZE __pragma(loop(novector))
    #define UNROLL_LOOP_HINT(n) __pragma(loop(unroll_count(n)))
#else
    #define VECTORIZE_LOOP
    #define VECTORIZE_ALWAYS
    #define NO_VECTORIZE
    #define UNROLL_LOOP_HINT(n)
#endif

// Loop fusion for consecutive loops over same data
template<typename T, size_t N>
class LoopFusion {
private:
    T* m_pData;
    size_t m_Size;
    
public:
    LoopFusion(T* data, size_t size) : m_pData(data), m_Size(size) {}
    
    // Apply multiple operations in single pass
    template<typename... Funcs>
    void Apply(Funcs&&... funcs) {
        for(size_t i = 0; i < m_Size; ++i) {
            T& element = m_pData[i];
            (funcs(element), ...); // C++17 fold expression
        }
    }
    
    // Apply with index
    template<typename... Funcs>
    void ApplyWithIndex(Funcs&&... funcs) {
        for(size_t i = 0; i < m_Size; ++i) {
            T& element = m_pData[i];
            (funcs(element, i), ...);
        }
    }
};

// Loop tiling for cache optimization
template<size_t TileSize = 64>
class LoopTiling {
public:
    template<typename Func>
    static void Execute2D(size_t width, size_t height, Func&& func) {
        for(size_t y = 0; y < height; y += TileSize) {
            size_t y_end = (y + TileSize < height) ? y + TileSize : height;
            
            for(size_t x = 0; x < width; x += TileSize) {
                size_t x_end = (x + TileSize < width) ? x + TileSize : width;
                
                // Process tile
                for(size_t ty = y; ty < y_end; ++ty) {
                    for(size_t tx = x; tx < x_end; ++tx) {
                        func(tx, ty);
                    }
                }
            }
        }
    }
    
    template<typename Func>
    static void Execute1D(size_t size, Func&& func) {
        for(size_t i = 0; i < size; i += TileSize) {
            size_t end = (i + TileSize < size) ? i + TileSize : size;
            
            for(size_t j = i; j < end; ++j) {
                func(j);
            }
        }
    }
};

// Loop interchange for better cache locality
template<typename Func>
static void InterchangeLoops(size_t outer_size, size_t inner_size, Func&& func) {
    // Original: for i in outer, for j in inner
    // Interchanged: for j in inner, for i in outer
    for(size_t j = 0; j < inner_size; ++j) {
        for(size_t i = 0; i < outer_size; ++i) {
            func(i, j);
        }
    }
}

// Loop skewing for dependency resolution
template<typename Func>
static void SkewLoops(size_t n, size_t m, Func&& func) {
    for(size_t k = 0; k < n + m - 1; ++k) {
        size_t i_start = (k >= m - 1) ? k - (m - 1) : 0;
        size_t i_end = (k < n - 1) ? k : n - 1;
        
        for(size_t i = i_start; i <= i_end; ++i) {
            size_t j = k - i;
            func(i, j);
        }
    }
}

// Loop parallelization hints
#if defined(_OPENMP)
    #include <omp.h>
    #define PARALLEL_FOR _Pragma("omp parallel for")
    #define PARALLEL_FOR_SIMD _Pragma("omp parallel for simd")
    #define PARALLEL_SECTIONS _Pragma("omp parallel sections")
    #define PARALLEL_SECTION _Pragma("omp section")
#else
    #define PARALLEL_FOR
    #define PARALLEL_FOR_SIMD
    #define PARALLEL_SECTIONS
    #define PARALLEL_SECTION
#endif

// SIMD vector types (if available)
#if defined(__SSE__) || defined(__AVX__)
    #include <xmmintrin.h>
    #include <emmintrin.h>
    
    class SIMDVector {
    private:
        __m128 m_Data;
        
    public:
        SIMDVector() : m_Data(_mm_setzero_ps()) {}
        SIMDVector(float f) : m_Data(_mm_set1_ps(f)) {}
        SIMDVector(float a, float b, float c, float d) : m_Data(_mm_set_ps(d, c, b, a)) {}
        SIMDVector(__m128 data) : m_Data(data) {}
        
        SIMDVector operator+(const SIMDVector& other) const {
            return SIMDVector(_mm_add_ps(m_Data, other.m_Data));
        }
        
        SIMDVector operator-(const SIMDVector& other) const {
            return SIMDVector(_mm_sub_ps(m_Data, other.m_Data));
        }
        
        SIMDVector operator*(const SIMDVector& other) const {
            return SIMDVector(_mm_mul_ps(m_Data, other.m_Data));
        }
        
        SIMDVector operator/(const SIMDVector& other) const {
            return SIMDVector(_mm_div_ps(m_Data, other.m_Data));
        }
        
        void Store(float* dest) const {
            _mm_store_ps(dest, m_Data);
        }
        
        static SIMDVector Load(const float* src) {
            return SIMDVector(_mm_load_ps(src));
        }
        
        static SIMDVector LoadUnaligned(const float* src) {
            return SIMDVector(_mm_loadu_ps(src));
        }
    };
    
    // SIMD-enabled loop
    template<typename Func>
    static void SIMDLoop(size_t count, Func&& func) {
        size_t i = 0;
        const size_t simd_width = 4;
        
        // SIMD loop
        for(; i + simd_width <= count; i += simd_width) {
            func(i, simd_width);
        }
        
        // Scalar remainder
        for(; i < count; ++i) {
            func(i, 1);
        }
    }
#endif

// Loop performance monitoring
class LoopTimer {
private:
    const char* m_Name;
    size_t m_Iterations;
    double m_StartTime;
    
    static double GetTime() {
        #ifdef _WIN32
            LARGE_INTEGER freq, count;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&count);
            return double(count.QuadPart) / double(freq.QuadPart);
        #else
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return ts.tv_sec + ts.tv_nsec * 1e-9;
        #endif
    }
    
public:
    LoopTimer(const char* name, size_t iterations) 
        : m_Name(name), m_Iterations(iterations) {
        m_StartTime = GetTime();
    }
    
    ~LoopTimer() {
        double endTime = GetTime();
        double elapsed = endTime - m_StartTime;
        double iterationsPerSec = m_Iterations / elapsed;
        
        // Log or store metrics
        // In production, you'd want to use a proper logging system
    }
};

// Loop invariant code motion helper
template<typename T>
class InvariantCode {
private:
    T m_Value;
    bool m_Calculated;
    
public:
    InvariantCode() : m_Calculated(false) {}
    
    template<typename Func>
    T& Get(Func&& calc_func) {
        if(!m_Calculated) {
            m_Value = calc_func();
            m_Calculated = true;
        }
        return m_Value;
    }
    
    void Reset() {
        m_Calculated = false;
    }
};

// Loop strength reduction
template<typename T>
class StrengthReduction {
public:
    // Convert multiplication to addition in loops
    template<typename Func>
    static void MultiplyToAdd(size_t count, T start, T step, Func&& func) {
        T value = start;
        for(size_t i = 0; i < count; ++i) {
            func(i, value);
            value += step;
        }
    }
    
    // Convert division to multiplication
    template<typename Func>
    static void DivideToMultiply(size_t count, T start, T inv_step, Func&& func) {
        T value = start;
        for(size_t i = 0; i < count; ++i) {
            func(i, value);
            value *= inv_step;
        }
    }
};

} // namespace loop_opt

// Convenience macros
#define LOOP_UNROLL(n, i, body) \
    for(size_t _unroll_idx = 0; _unroll_idx < (n); ++_unroll_idx) { \
        size_t i = _unroll_idx; \
        body \
    }

#define LOOP_UNROLL_PARTIAL(factor, count, i, body) \
    loop_opt::PartialUnroll<factor>::Execute(count, [&](size_t i) { body })

#define LOOP_TILED_2D(tile, width, height, x, y, body) \
    loop_opt::LoopTiling<tile>::Execute2D(width, height, [&](size_t x, size_t y) { body })

#define LOOP_FUSION(data, size, ...) \
    loop_opt::LoopFusion<decltype(*data), size> fusion(data, size); \
    fusion.Apply(__VA_ARGS__)

#endif // BASE_TL_LOOP_OPTIMIZATION_H