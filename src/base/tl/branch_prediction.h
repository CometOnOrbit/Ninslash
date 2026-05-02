#ifndef BASE_TL_BRANCH_PREDICTION_H
#define BASE_TL_BRANCH_PREDICTION_H

#include <cstdint>

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

// Prefetch hints for branch prediction
#if defined(__GNUC__) || defined(__clang__)
    #define PREFETCH_READ(addr)  __builtin_prefetch(addr, 0, 3)
    #define PREFETCH_WRITE(addr) __builtin_prefetch(addr, 1, 3)
#else
    #define PREFETCH_READ(addr)
    #define PREFETCH_WRITE(addr)
#endif

namespace branch_opt {

// Branchless selection (like ternary operator but always evaluates both sides)
template<typename T>
inline T Select(bool condition, T true_value, T false_value)
{
    // Branchless using arithmetic
    return condition ? true_value : false_value;
}

// Branchless absolute value for floats
inline float BranchlessAbs(float x)
{
    // Clear sign bit
    union { float f; uint32_t i; } u;
    u.f = x;
    u.i &= 0x7FFFFFFF;
    return u.f;
}

// Branchless sign copy
inline float BranchlessCopySign(float x, float y)
{
    union { float f; uint32_t i; } ux, uy;
    ux.f = x;
    uy.f = y;
    ux.i = (ux.i & 0x7FFFFFFF) | (uy.i & 0x80000000);
    return ux.f;
}

// Branchless min for floats
inline float BranchlessMin(float a, float b)
{
    // Use integer comparison to avoid branching
    union { float f; uint32_t i; } ua, ub;
    ua.f = a;
    ub.f = b;
    
    uint32_t mask = (int32_t)(ua.i - ub.i) >> 31;
    
    // Need to use integer operations, not float bitwise ops
    union { float f; uint32_t i; } result;
    result.i = (ua.i & ~mask) | (ub.i & mask);
    return result.f;
}

// Branchless max for floats
inline float BranchlessMax(float a, float b)
{
    union { float f; uint32_t i; } ua, ub;
    ua.f = a;
    ub.f = b;
    
    uint32_t mask = (int32_t)(ub.i - ua.i) >> 31;
    
    union { float f; uint32_t i; } result;
    result.i = (ua.i & ~mask) | (ub.i & mask);
    return result.f;
}

// Branchless clamp for floats
inline float BranchlessClamp(float value, float min_val, float max_val)
{
    float temp = BranchlessMax(value, min_val);
    return BranchlessMin(temp, max_val);
}

// Convert boolean to integer without branching
inline int32_t BoolToInt(bool b)
{
    return static_cast<int32_t>(b);
}

inline int32_t BoolToIntBranchless(bool b)
{
    return -static_cast<int32_t>(b);
}

// Convert integer to boolean without branching
inline bool IntToBool(int32_t i)
{
    return i != 0;
}

inline bool IntToBoolBranchless(int32_t i)
{
    return static_cast<bool>(i & 1);
}

// Count number of true values in boolean array (branchless)
template<size_t N>
inline size_t CountTrueBranchless(const bool (&array)[N])
{
    size_t count = 0;
    for(size_t i = 0; i < N; ++i)
    {
        count += static_cast<size_t>(array[i]);
    }
    return count;
}

// Find first true value (branchless version)
template<size_t N>
inline int FindFirstTrueBranchless(const bool (&array)[N])
{
    for(size_t i = 0; i < N; ++i)
    {
        if(array[i]) return static_cast<int>(i);
    }
    return -1;
}

// Branchless array lookup with bounds checking
template<typename T, size_t N>
inline T& SafeArrayAccess(T (&array)[N], size_t index)
{
    // Branchless bounds check
    index = index < N ? index : N - 1;
    return array[index];
}

// Hot/Cold function attributes for better code layout
#if defined(__GNUC__) || defined(__clang__)
    #define HOT_FUNCTION __attribute__((hot))
    #define COLD_FUNCTION __attribute__((cold))
    #define FLATTEN_FUNCTION __attribute__((flatten))
    #define NOINLINE_FUNCTION __attribute__((noinline))
    #define ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
    #define HOT_FUNCTION __declspec(noinline)
    #define COLD_FUNCTION __declspec(noinline)
    #define FLATTEN_FUNCTION
    #define NOINLINE_FUNCTION __declspec(noinline)
    #define ALWAYS_INLINE __forceinline
#else
    #define HOT_FUNCTION
    #define COLD_FUNCTION
    #define FLATTEN_FUNCTION
    #define NOINLINE_FUNCTION
    #define ALWAYS_INLINE inline
#endif

// Expected value macros for common patterns
#define EXPECT_TRUE(cond)   LIKELY(cond)
#define EXPECT_FALSE(cond)  UNLIKELY(cond)
#define EXPECT_NULL(ptr)    UNLIKELY((ptr) == nullptr)
#define EXPECT_NOT_NULL(ptr) LIKELY((ptr) != nullptr)
#define EXPECT_ZERO(val)    UNLIKELY((val) == 0)
#define EXPECT_NONZERO(val) LIKELY((val) != 0)
#define EXPECT_SUCCESS(err) LIKELY((err) == 0)
#define EXPECT_FAILURE(err) UNLIKELY((err) != 0)

// Loop optimization hints
#define LOOP_LIKELY(cond)   for(; LIKELY(cond); )
#define LOOP_UNLIKELY(cond) for(; UNLIKELY(cond); )

// Early return pattern for error handling
#define RETURN_IF_UNLIKELY(cond, retval) \
    do { \
        if(UNLIKELY(cond)) { \
            return retval; \
        } \
    } while(0)

#define RETURN_IF_LIKELY(cond, retval) \
    do { \
        if(LIKELY(cond)) { \
            return retval; \
        } \
    } while(0)

// Switch statement optimization
template<typename T>
inline bool IsPowerOfTwoSwitch(T value)
{
    switch(value)
    {
        case 1: case 2: case 4: case 8: case 16:
        case 32: case 64: case 128: case 256:
        case 512: case 1024: case 2048: case 4096:
        case 8192: case 16384: case 32768: case 65536:
            return true;
        default:
            return false;
    }
}

// Table-based dispatch for small value ranges
template<typename Func, size_t N>
inline auto TableDispatch(int value, const Func (&table)[N]) -> decltype(table[0]())
{
    if(UNLIKELY(value < 0 || value >= static_cast<int>(N)))
        return table[0](); // Default
    
    return table[value]();
}

// Probability hints for compiler
struct Probability
{
    static constexpr float ALWAYS = 1.0f;
    static constexpr float NEVER = 0.0f;
    static constexpr float VERY_LIKELY = 0.9f;
    static constexpr float LIKELY = 0.7f;
    static constexpr float UNLIKELY = 0.3f;
    static constexpr float VERY_UNLIKELY = 0.1f;
};

// Profile-guided optimization hints
#if defined(__GNUC__) || defined(__clang__)
    #define PROFILE_LIKELY(prob)   __builtin_expect_with_probability(!!(cond), 1, prob)
    #define PROFILE_UNLIKELY(prob) __builtin_expect_with_probability(!!(cond), 0, prob)
#else
    #define PROFILE_LIKELY(prob)   (cond)
    #define PROFILE_UNLIKELY(prob) (cond)
#endif

} // namespace branch_opt

// Convenience macros
#define BRANCHLESS_ABS(x) branch_opt::BranchlessAbs(x)
#define BRANCHLESS_MIN(a, b) branch_opt::BranchlessMin(a, b)
#define BRANCHLESS_MAX(a, b) branch_opt::BranchlessMax(a, b)
#define BRANCHLESS_CLAMP(val, min, max) branch_opt::BranchlessClamp(val, min, max)
#define BRANCHLESS_COPYSIGN(x, y) branch_opt::BranchlessCopySign(x, y)

#endif // BASE_TL_BRANCH_PREDICTION_H