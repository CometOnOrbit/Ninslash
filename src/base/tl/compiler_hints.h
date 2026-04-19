#ifndef BASE_TL_COMPILER_HINTS_H
#define BASE_TL_COMPILER_HINTS_H

#include <cstddef>
#include <type_traits>

namespace compiler_hints {

// Compiler-specific pragma wrappers
class PragmaOptimizer {
public:
    // Optimization level
    static void PushOptimizationLevel(int level) {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC push_options")
            _Pragma("GCC optimize(\"O3\")")
        #elif defined(_MSC_VER)
            __pragma(optimize("", on))
        #endif
    }
    
    static void PopOptimizationLevel() {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC pop_options")
        #elif defined(_MSC_VER)
            __pragma(optimize("", off))
        #endif
    }
    
    // Inline control
    static void ForceInline() {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC always_inline")
        #elif defined(_MSC_VER)
            __pragma(inline_depth(255))
            __pragma(inline_recursion(on))
        #endif
    }
    
    static void NoInline() {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC noinline")
        #elif defined(_MSC_VER)
            __pragma(inline_depth(0))
            __pragma(inline_recursion(off))
        #endif
    }
    
    // Loop optimization
    static void UnrollLoop(size_t factor) {
        #if defined(__GNUC__) || defined(__clang__)
            if(factor == 0) {
                _Pragma("GCC unroll")
            } else {
                switch(factor) {
                    case 2: _Pragma("GCC unroll 2"); break;
                    case 4: _Pragma("GCC unroll 4"); break;
                    case 8: _Pragma("GCC unroll 8"); break;
                    case 16: _Pragma("GCC unroll 16"); break;
                    default: _Pragma("GCC unroll"); break;
                }
            }
        #elif defined(_MSC_VER)
            __pragma(loop(unroll_count(factor)))
        #endif
    }
    
    static void NoUnroll() {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC nounroll")
        #elif defined(_MSC_VER)
            __pragma(loop(unroll_count(1)))
        #endif
    }
    
    // Vectorization
    static void Vectorize() {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC ivdep")
        #elif defined(_MSC_VER)
            __pragma(loop(ivdep))
        #endif
    }
    
    static void NoVectorize() {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC novector")
        #elif defined(_MSC_VER)
            __pragma(loop(novector))
        #endif
    }
    
    // Pipeline optimization
    static void Pipeline() {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC pipeline")
        #endif
    }
    
    // Branch prediction
    static void LikelyBranch() {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC predict likely")
        #endif
    }
    
    static void UnlikelyBranch() {
        #if defined(__GNUC__) || defined(__clang__)
            _Pragma("GCC predict unlikely")
        #endif
    }
};

// Function attributes
class FunctionAttributes {
public:
    // Hot function (frequently executed)
    template<typename Func>
    static Func* MakeHot(Func* func) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((hot)) return func;
        #elif defined(_MSC_VER)
            __declspec(noinline) return func;
        #else
            return func;
        #endif
    }
    
    // Cold function (rarely executed)
    template<typename Func>
    static Func* MakeCold(Func* func) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((cold)) return func;
        #elif defined(_MSC_VER)
            __declspec(noinline) return func;
        #else
            return func;
        #endif
    }
    
    // Pure function (no side effects, depends only on parameters)
    template<typename Func>
    static Func* MakePure(Func* func) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((pure)) return func;
        #else
            return func;
        #endif
    }
    
    // Const function (no side effects, depends only on parameters, doesn't read global memory)
    template<typename Func>
    static Func* MakeConst(Func* func) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((const)) return func;
        #else
            return func;
        #endif
    }
    
    // No return function
    template<typename Func>
    static Func* MakeNoReturn(Func* func) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((noreturn)) return func;
        #elif defined(_MSC_VER)
            __declspec(noreturn) return func;
        #else
            return func;
        #endif
    }
    
    // No throw function
    template<typename Func>
    static Func* MakeNoThrow(Func* func) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((nothrow)) return func;
        #elif defined(_MSC_VER)
            __declspec(nothrow) return func;
        #else
            return func;
        #endif
    }
    
    // Returns twice (like setjmp)
    template<typename Func>
    static Func* MakeReturnsTwice(Func* func) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((returns_twice)) return func;
        #else
            return func;
        #endif
    }
    
    // Flatten function (inline all calls inside)
    template<typename Func>
    static Func* MakeFlatten(Func* func) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((flatten)) return func;
        #else
            return func;
        #endif
    }
};

// Variable attributes
class VariableAttributes {
public:
    // Aligned variable
    template<typename T>
    static T* MakeAligned(T* var, size_t alignment) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((aligned(alignment))) return var;
        #elif defined(_MSC_VER)
            __declspec(align(alignment)) return var;
        #else
            return var;
        #endif
    }
    
    // Packed structure (no padding)
    template<typename T>
    static T* MakePacked(T* var) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((packed)) return var;
        #elif defined(_MSC_VER)
            __pragma(pack(push, 1)) return var;
        #else
            return var;
        #endif
    }
    
    // Cleanup attribute (auto cleanup)
    template<typename T, typename CleanupFunc>
    static T* MakeCleanup(T* var, CleanupFunc cleanup) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((cleanup(cleanup))) return var;
        #else
            return var;
        #endif
    }
    
    // Unused variable (suppress warnings)
    template<typename T>
    static T* MakeUnused(T* var) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((unused)) return var;
        #elif defined(_MSC_VER)
            __pragma(warning(suppress: 4100 4101)) return var;
        #else
            return var;
        #endif
    }
    
    // Used variable (prevent removal)
    template<typename T>
    static T* MakeUsed(T* var) {
        #if defined(__GNUC__) || defined(__clang__)
            __attribute__((used)) return var;
        #else
            return var;
        #endif
    }
};

// Optimization hints
class OptimizationHints {
public:
    // Assume value is not null
    template<typename T>
    static T* AssumeNotNull(T* ptr) {
        #if defined(__GNUC__) || defined(__clang__)
            if(__builtin_expect(ptr != nullptr, 1)) {
                return ptr;
            }
            __builtin_unreachable();
        #else
            return ptr;
        #endif
    }
    
    // Assume value is null
    template<typename T>
    static T* AssumeNull(T* ptr) {
        #if defined(__GNUC__) || defined(__clang__)
            if(__builtin_expect(ptr == nullptr, 1)) {
                return ptr;
            }
            __builtin_unreachable();
        #else
            return ptr;
        #endif
    }
    
    // Assume condition is true
    static bool AssumeTrue(bool condition) {
        #if defined(__GNUC__) || defined(__clang__)
            if(__builtin_expect(condition, 1)) {
                return condition;
            }
            __builtin_unreachable();
        #else
            return condition;
        #endif
    }
    
    // Assume condition is false
    static bool AssumeFalse(bool condition) {
        #if defined(__GNUC__) || defined(__clang__)
            if(__builtin_expect(!condition, 1)) {
                return condition;
            }
            __builtin_unreachable();
        #else
            return condition;
        #endif
    }
    
    // Assume value is aligned
    template<typename T>
    static T* AssumeAligned(T* ptr, size_t alignment) {
        #if defined(__GNUC__) || defined(__clang__)
            return (T*)__builtin_assume_aligned(ptr, alignment);
        #else
            return ptr;
        #endif
    }
    
    // Mark unreachable code
    static void Unreachable() {
        #if defined(__GNUC__) || defined(__clang__)
            __builtin_unreachable();
        #elif defined(_MSC_VER)
            __assume(0);
        #endif
    }
    
    // Expect value (branch prediction)
    template<typename T>
    static T Expect(T value, T expected) {
        #if defined(__GNUC__) || defined(__clang__)
            return __builtin_expect(value, expected);
        #else
            return value;
        #endif
    }
    
    // Expect with probability
    template<typename T>
    static T ExpectWithProbability(T value, T expected, double probability) {
        #if defined(__GNUC__) && __GNUC__ >= 9
            return __builtin_expect_with_probability(value, expected, probability);
        #else
            return Expect(value, expected);
        #endif
    }
};

// Compiler-specific intrinsics
class CompilerIntrinsics {
public:
    // Population count
    static int PopCount32(uint32_t x) {
        #if defined(__GNUC__) || defined(__clang__)
            return __builtin_popcount(x);
        #elif defined(_MSC_VER)
            return __popcnt(x);
        #else
            // Software fallback
            x = x - ((x >> 1) & 0x55555555);
            x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
            x = (x + (x >> 4)) & 0x0F0F0F0F;
            x = x + (x >> 8);
            x = x + (x >> 16);
            return x & 0x3F;
        #endif
    }
    
    static int PopCount64(uint64_t x) {
        #if defined(__GNUC__) || defined(__clang__)
            return __builtin_popcountll(x);
        #elif defined(_MSC_VER) && defined(_WIN64)
            return __popcnt64(x);
        #else
            return PopCount32((uint32_t)x) + PopCount32((uint32_t)(x >> 32));
        #endif
    }
    
    // Count leading zeros
    static int CountLeadingZeros32(uint32_t x) {
        #if defined(__GNUC__) || defined(__clang__)
            return x == 0 ? 32 : __builtin_clz(x);
        #elif defined(_MSC_VER)
            unsigned long index;
            return _BitScanReverse(&index, x) ? 31 - index : 32;
        #else
            if(x == 0) return 32;
            int n = 0;
            if(x <= 0x0000FFFF) { n += 16; x <<= 16; }
            if(x <= 0x00FFFFFF) { n += 8; x <<= 8; }
            if(x <= 0x0FFFFFFF) { n += 4; x <<= 4; }
            if(x <= 0x3FFFFFFF) { n += 2; x <<= 2; }
            if(x <= 0x7FFFFFFF) { n += 1; }
            return n;
        #endif
    }
    
    // Count trailing zeros
    static int CountTrailingZeros32(uint32_t x) {
        #if defined(__GNUC__) || defined(__clang__)
            return x == 0 ? 32 : __builtin_ctz(x);
        #elif defined(_MSC_VER)
            unsigned long index;
            return _BitScanForward(&index, x) ? index : 32;
        #else
            if(x == 0) return 32;
            int n = 0;
            if((x & 0x0000FFFF) == 0) { n += 16; x >>= 16; }
            if((x & 0x000000FF) == 0) { n += 8; x >>= 8; }
            if((x & 0x0000000F) == 0) { n += 4; x >>= 4; }
            if((x & 0x00000003) == 0) { n += 2; x >>= 2; }
            if((x & 0x00000001) == 0) { n += 1; }
            return n;
        #endif
    }
    
    // Byte swap
    static uint32_t ByteSwap32(uint32_t x) {
        #if defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap32(x);
        #elif defined(_MSC_VER)
            return _byteswap_ulong(x);
        #else
            return ((x & 0xFF000000) >> 24) |
                   ((x & 0x00FF0000) >> 8) |
                   ((x & 0x0000FF00) << 8) |
                   ((x & 0x000000FF) << 24);
        #endif
    }
    
    static uint64_t ByteSwap64(uint64_t x) {
        #if defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap64(x);
        #elif defined(_MSC_VER)
            return _byteswap_uint64(x);
        #else
            return ((x & 0xFF00000000000000ULL) >> 56) |
                   ((x & 0x00FF000000000000ULL) >> 40) |
                   ((x & 0x0000FF0000000000ULL) >> 24) |
                   ((x & 0x000000FF00000000ULL) >> 8) |
                   ((x & 0x00000000FF000000ULL) << 8) |
                   ((x & 0x0000000000FF0000ULL) << 24) |
                   ((x & 0x000000000000FF00ULL) << 40) |
                   ((x & 0x00000000000000FFULL) << 56);
        #endif
    }
    
    // Prefetch
    static void Prefetch(const void* addr, int rw = 0, int locality = 3) {
        #if defined(__GNUC__) || defined(__clang__)
            __builtin_prefetch(addr, rw, locality);
        #elif defined(_MSC_VER)
            _mm_prefetch((const char*)addr, _MM_HINT_T0);
        #endif
    }
};

// Compiler barrier macros
#define COMPILER_BARRIER() \
    do { \
        #if defined(__GNUC__) || defined(__clang__) \
            asm volatile("" ::: "memory"); \
        #elif defined(_MSC_VER) \
            _ReadWriteBarrier(); \
        #endif \
    } while(0)

#define MEMORY_BARRIER() \
    do { \
        #if defined(__GNUC__) || defined(__clang__) \
            __sync_synchronize(); \
        #elif defined(_MSC_VER) \
            _mm_mfence(); \
        #endif \
    } while(0)

// Optimization scope
class OptimizationScope {
public:
    OptimizationScope(int level = 3) {
        compiler_hints::PragmaOptimizer::PushOptimizationLevel(level);
    }
    
    ~OptimizationScope() {
        compiler_hints::PragmaOptimizer::PopOptimizationLevel();
    }
    
    // Disable copy
    OptimizationScope(const OptimizationScope&) = delete;
    OptimizationScope& operator=(const OptimizationScope&) = delete;
};

// Inline scope
class InlineScope {
public:
    InlineScope(bool force = true) {
        if(force) {
            compiler_hints::PragmaOptimizer::ForceInline();
        } else {
            compiler_hints::PragmaOptimizer::NoInline();
        }
    }
    
    ~InlineScope() {
        // Inline pragmas don't need popping
    }
};

// Loop optimization scope
class LoopOptimizationScope {
public:
    LoopOptimizationScope(size_t unroll_factor = 0, bool vectorize = true) {
        if(unroll_factor > 0) {
            compiler_hints::PragmaOptimizer::UnrollLoop(unroll_factor);
        } else {
            compiler_hints::PragmaOptimizer::NoUnroll();
        }
        
        if(vectorize) {
            compiler_hints::PragmaOptimizer::Vectorize();
        } else {
            compiler_hints::PragmaOptimizer::NoVectorize();
        }
    }
};

} // namespace compiler_hints

// Convenience macros
#define OPTIMIZATION_SCOPE(level) compiler_hints::OptimizationScope _opt_scope(level)
#define INLINE_SCOPE(force) compiler_hints::InlineScope _inline_scope(force)
#define LOOP_OPTIMIZATION_SCOPE(unroll, vectorize) compiler_hints::LoopOptimizationScope _loop_scope(unroll, vectorize)

#define HOT_FUNCTION compiler_hints::FunctionAttributes::MakeHot
#define COLD_FUNCTION compiler_hints::FunctionAttributes::MakeCold
#define PURE_FUNCTION compiler_hints::FunctionAttributes::MakePure
#define CONST_FUNCTION compiler_hints::FunctionAttributes::MakeConst

#define ASSUME_NOT_NULL(ptr) compiler_hints::OptimizationHints::AssumeNotNull(ptr)
#define ASSUME_NULL(ptr) compiler_hints::OptimizationHints::AssumeNull(ptr)
#define ASSUME_TRUE(cond) compiler_hints::OptimizationHints::AssumeTrue(cond)
#define ASSUME_FALSE(cond) compiler_hints::OptimizationHints::AssumeFalse(cond)
#define ASSUME_ALIGNED(ptr, alignment) compiler_hints::OptimizationHints::AssumeAligned(ptr, alignment)
#define UNREACHABLE() compiler_hints::OptimizationHints::Unreachable()

#define EXPECT(value, expected) compiler_hints::OptimizationHints::Expect(value, expected)
#define EXPECT_PROBABILITY(value, expected, prob) compiler_hints::OptimizationHints::ExpectWithProbability(value, expected, prob)

#define POPCOUNT32(x) compiler_hints::CompilerIntrinsics::PopCount32(x)
#define POPCOUNT64(x) compiler_hints::CompilerIntrinsics::PopCount64(x)
#define CLZ32(x) compiler_hints::CompilerIntrinsics::CountLeadingZeros32(x)
#define CTZ32(x) compiler_hints::CompilerIntrinsics::CountTrailingZeros32(x)
#define BSWAP32(x) compiler_hints::CompilerIntrinsics::ByteSwap32(x)
#define BSWAP64(x) compiler_hints::CompilerIntrinsics::ByteSwap64(x)
#define PREFETCH(addr) compiler_hints::CompilerIntrinsics::Prefetch(addr)

#endif // BASE_TL_COMPILER_HINTS_H