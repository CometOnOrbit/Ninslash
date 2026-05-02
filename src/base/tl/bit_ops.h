#ifndef BASE_TL_BIT_OPS_H
#define BASE_TL_BIT_OPS_H

#include <cstdint>
#include <type_traits>
#include <climits>

namespace bit_ops {

// Check if a number is power of two
template<typename T>
inline bool IsPowerOfTwo(T value)
{
    static_assert(std::is_unsigned<T>::value, "IsPowerOfTwo requires unsigned type");
    return value && !(value & (value - 1));
}

// Round up to next power of two
template<typename T>
inline T NextPowerOfTwo(T value)
{
    static_assert(std::is_unsigned<T>::value, "NextPowerOfTwo requires unsigned type");
    
    if(value == 0) return 1;
    
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    
    if(sizeof(T) > 4)
        value |= value >> 32;
    
    return value + 1;
}

// Count leading zeros (clz)
inline int CountLeadingZeros32(uint32_t x)
{
    if(x == 0) return 32;
    
    #if defined(__GNUC__) || defined(__clang__)
        return __builtin_clz(x);
    #elif defined(_MSC_VER)
        unsigned long index;
        _BitScanReverse(&index, x);
        return 31 - index;
    #else
        // Software fallback
        int n = 0;
        if(x <= 0x0000FFFF) { n += 16; x <<= 16; }
        if(x <= 0x00FFFFFF) { n += 8; x <<= 8; }
        if(x <= 0x0FFFFFFF) { n += 4; x <<= 4; }
        if(x <= 0x3FFFFFFF) { n += 2; x <<= 2; }
        if(x <= 0x7FFFFFFF) { n += 1; }
        return n;
    #endif
}

// Count trailing zeros (ctz)
inline int CountTrailingZeros32(uint32_t x)
{
    if(x == 0) return 32;
    
    #if defined(__GNUC__) || defined(__clang__)
        return __builtin_ctz(x);
    #elif defined(_MSC_VER)
        unsigned long index;
        _BitScanForward(&index, x);
        return index;
    #else
        // Software fallback
        int n = 0;
        if((x & 0x0000FFFF) == 0) { n += 16; x >>= 16; }
        if((x & 0x000000FF) == 0) { n += 8; x >>= 8; }
        if((x & 0x0000000F) == 0) { n += 4; x >>= 4; }
        if((x & 0x00000003) == 0) { n += 2; x >>= 2; }
        if((x & 0x00000001) == 0) { n += 1; }
        return n;
    #endif
}

// Population count (popcount) - count set bits
inline int PopulationCount32(uint32_t x)
{
    #if defined(__GNUC__) || defined(__clang__)
        return __builtin_popcount(x);
    #elif defined(_MSC_VER)
        return __popcnt(x);
    #else
        // Software fallback (SWAR algorithm)
        x = x - ((x >> 1) & 0x55555555);
        x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
        x = (x + (x >> 4)) & 0x0F0F0F0F;
        x = x + (x >> 8);
        x = x + (x >> 16);
        return x & 0x3F;
    #endif
}

// Fast modulo for power of two divisors
template<typename T>
inline T FastModPowerOfTwo(T value, T divisor)
{
    static_assert(std::is_unsigned<T>::value, "FastModPowerOfTwo requires unsigned type");
    return value & (divisor - 1);
}

// Fast division by power of two (right shift)
template<typename T>
inline T FastDivPowerOfTwo(T value, int shift)
{
    static_assert(std::is_integral<T>::value, "FastDivPowerOfTwo requires integral type");
    return value >> shift;
}

// Fast multiplication by power of two (left shift)
template<typename T>
inline T FastMulPowerOfTwo(T value, int shift)
{
    static_assert(std::is_integral<T>::value, "FastMulPowerOfTwo requires integral type");
    return value << shift;
}

// Fast ceiling division
template<typename T>
inline T FastCeilDiv(T numerator, T denominator)
{
    static_assert(std::is_integral<T>::value, "FastCeilDiv requires integral type");
    return (numerator + denominator - 1) / denominator;
}

// Fast floor division (standard integer division)
template<typename T>
inline T FastFloorDiv(T numerator, T denominator)
{
    static_assert(std::is_integral<T>::value, "FastFloorDiv requires integral type");
    return numerator / denominator;
}

// Fast absolute value for integers (branchless)
template<typename T>
inline T FastAbs(T value)
{
    static_assert(std::is_integral<T>::value, "FastAbs requires integral type");
    
    if constexpr(std::is_signed<T>::value)
    {
        T mask = value >> (sizeof(T) * CHAR_BIT - 1);
        return (value + mask) ^ mask;
    }
    else
    {
        return value; // Unsigned is always positive
    }
}

// Fast min (branchless)
template<typename T>
inline T FastMin(T a, T b)
{
    return b ^ ((a ^ b) & -(a < b));
}

// Fast max (branchless)
template<typename T>
inline T FastMax(T a, T b)
{
    return a ^ ((a ^ b) & -(a < b));
}

// Fast clamp (branchless)
template<typename T>
inline T FastClamp(T value, T min_val, T max_val)
{
    T temp = value < min_val ? min_val : value;
    return temp > max_val ? max_val : temp;
}

// Rotate left
template<typename T>
inline T RotateLeft(T value, int shift)
{
    static_assert(std::is_unsigned<T>::value, "RotateLeft requires unsigned type");
    constexpr int bits = sizeof(T) * CHAR_BIT;
    shift &= bits - 1;
    return (value << shift) | (value >> (bits - shift));
}

// Rotate right
template<typename T>
inline T RotateRight(T value, int shift)
{
    static_assert(std::is_unsigned<T>::value, "RotateRight requires unsigned type");
    constexpr int bits = sizeof(T) * CHAR_BIT;
    shift &= bits - 1;
    return (value >> shift) | (value << (bits - shift));
}

// Swap two values without temporary (XOR swap)
template<typename T>
inline void XorSwap(T& a, T& b)
{
    static_assert(std::is_integral<T>::value, "XorSwap requires integral type");
    if(&a != &b)
    {
        a ^= b;
        b ^= a;
        a ^= b;
    }
}

// Check if number is even (branchless)
template<typename T>
inline bool IsEven(T value)
{
    static_assert(std::is_integral<T>::value, "IsEven requires integral type");
    return (value & 1) == 0;
}

// Check if number is odd (branchless)
template<typename T>
inline bool IsOdd(T value)
{
    static_assert(std::is_integral<T>::value, "IsOdd requires integral type");
    return (value & 1) == 1;
}

// Extract bit field
template<typename T>
inline T ExtractBitField(T value, int start, int length)
{
    static_assert(std::is_unsigned<T>::value, "ExtractBitField requires unsigned type");
    return (value >> start) & ((1 << length) - 1);
}

// Set bit field
template<typename T>
inline T SetBitField(T value, T field, int start, int length)
{
    static_assert(std::is_unsigned<T>::value, "SetBitField requires unsigned type");
    T mask = ((1 << length) - 1) << start;
    return (value & ~mask) | ((field << start) & mask);
}

// Fast log2 for integers (floor)
inline int FastLog2(uint32_t x)
{
    return 31 - CountLeadingZeros32(x | 1);
}

// Fast log2 for integers (ceil)
inline int FastLog2Ceil(uint32_t x)
{
    return 32 - CountLeadingZeros32(x - 1);
}

// Fast reciprocal approximation for floats (for division)
inline float FastReciprocal(float x)
{
    // Use bit manipulation for approximate reciprocal
    union { float f; uint32_t i; } u;
    u.f = x;
    
    // Initial approximation: 1/x ≈ magic_constant - exponent
    uint32_t magic = 0x7EF311C2; // Magic constant for reciprocal
    u.i = magic - u.i;
    
    // One Newton-Raphson iteration for better accuracy
    float r = u.f;
    return r * (2.0f - x * r);
}

} // namespace bit_ops

// Convenience macros
#define IS_POW2(x) bit_ops::IsPowerOfTwo(x)
#define NEXT_POW2(x) bit_ops::NextPowerOfTwo(x)
#define FAST_MOD_POW2(value, divisor) bit_ops::FastModPowerOfTwo(value, divisor)
#define FAST_DIV_POW2(value, shift) bit_ops::FastDivPowerOfTwo(value, shift)
#define FAST_MUL_POW2(value, shift) bit_ops::FastMulPowerOfTwo(value, shift)
#define FAST_ABS(x) bit_ops::FastAbs(x)
#define FAST_MIN(a, b) bit_ops::FastMin(a, b)
#define FAST_MAX(a, b) bit_ops::FastMax(a, b)
#define FAST_CLAMP(value, min_val, max_val) bit_ops::FastClamp(value, min_val, max_val)
#define IS_EVEN(x) bit_ops::IsEven(x)
#define IS_ODD(x) bit_ops::IsOdd(x)

#endif // BASE_TL_BIT_OPS_H