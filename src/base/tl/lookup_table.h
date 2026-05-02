#ifndef BASE_TL_LOOKUP_TABLE_H
#define BASE_TL_LOOKUP_TABLE_H

#include <cmath>
#include <cstdint>
#include <cstring>

// Lookup table for trigonometric functions
class CTrigLUT
{
private:
    static const int TABLE_SIZE = 4096; // 4K entries, good precision
    static const int TABLE_MASK = TABLE_SIZE - 1;
    
    float m_SinTable[TABLE_SIZE];
    float m_CosTable[TABLE_SIZE];
    float m_TanTable[TABLE_SIZE];
    float m_InvSinTable[TABLE_SIZE]; // 1/sin for faster calculations
    float m_InvCosTable[TABLE_SIZE]; // 1/cos
    
    // Singleton instance
    static CTrigLUT* s_pInstance;
    
    CTrigLUT()
    {
        BuildTables();
    }
    
    void BuildTables()
    {
        const float PI = 3.14159265358979323846f;
        const float TWO_PI = 2.0f * PI;
        const float HALF_PI = 0.5f * PI;
        
        for(int i = 0; i < TABLE_SIZE; ++i)
        {
            float angle = (i * TWO_PI) / TABLE_SIZE;
            m_SinTable[i] = sinf(angle);
            m_CosTable[i] = cosf(angle);
            m_TanTable[i] = tanf(angle);
            
            // Precompute inverses (with safety checks)
            float sin_val = m_SinTable[i];
            float cos_val = m_CosTable[i];
            
            m_InvSinTable[i] = (fabsf(sin_val) > 1e-6f) ? (1.0f / sin_val) : 0.0f;
            m_InvCosTable[i] = (fabsf(cos_val) > 1e-6f) ? (1.0f / cos_val) : 0.0f;
        }
    }
    
public:
    // Delete copy constructor and assignment
    CTrigLUT(const CTrigLUT&) = delete;
    CTrigLUT& operator=(const CTrigLUT&) = delete;
    
    static CTrigLUT& Get()
    {
        if(!s_pInstance)
        {
            static CTrigLUT instance;
            s_pInstance = &instance;
        }
        return *s_pInstance;
    }
    
    // Convert angle to table index
    inline int AngleToIndex(float angle) const
    {
        const float TWO_PI = 2.0f * 3.14159265358979323846f;
        
        // Normalize angle to [0, 2π)
        angle = fmodf(angle, TWO_PI);
        if(angle < 0) angle += TWO_PI;
        
        return (int)((angle * TABLE_SIZE) / TWO_PI) & TABLE_MASK;
    }
    
    // Fast sine using lookup table
    inline float FastSin(float angle) const
    {
        return m_SinTable[AngleToIndex(angle)];
    }
    
    // Fast cosine using lookup table
    inline float FastCos(float angle) const
    {
        return m_CosTable[AngleToIndex(angle)];
    }
    
    // Fast tangent using lookup table
    inline float FastTan(float angle) const
    {
        return m_TanTable[AngleToIndex(angle)];
    }
    
    // Fast inverse sine (1/sin)
    inline float FastInvSin(float angle) const
    {
        return m_InvSinTable[AngleToIndex(angle)];
    }
    
    // Fast inverse cosine (1/cos)
    inline float FastInvCos(float angle) const
    {
        return m_InvCosTable[AngleToIndex(angle)];
    }
    
    // Fast atan2 approximation using lookup table
    inline float FastAtan2(float y, float x) const
    {
        const float PI = 3.14159265358979323846f;
        const float HALF_PI = 0.5f * PI;
        
        if(x == 0.0f)
        {
            if(y > 0.0f) return HALF_PI;
            if(y < 0.0f) return -HALF_PI;
            return 0.0f;
        }
        
        float ratio = y / x;
        float angle = atanf(ratio);
        
        if(x < 0.0f)
        {
            if(y >= 0.0f) angle += PI;
            else angle -= PI;
        }
        
        return angle;
    }
    
    // Fast sin/cos together (saves index calculation)
    inline void FastSinCos(float angle, float& sin_out, float& cos_out) const
    {
        int idx = AngleToIndex(angle);
        sin_out = m_SinTable[idx];
        cos_out = m_CosTable[idx];
    }
    
    // Linear interpolation between table entries for higher precision
    inline float FastSinLerp(float angle) const
    {
        const float TWO_PI = 2.0f * 3.14159265358979323846f;
        
        // Normalize angle
        angle = fmodf(angle, TWO_PI);
        if(angle < 0) angle += TWO_PI;
        
        // Calculate exact table position
        float pos = (angle * TABLE_SIZE) / TWO_PI;
        int idx0 = (int)pos & TABLE_MASK;
        int idx1 = (idx0 + 1) & TABLE_MASK;
        float t = pos - (int)pos;
        
        // Linear interpolation
        return m_SinTable[idx0] * (1.0f - t) + m_SinTable[idx1] * t;
    }
    
    // Same for cosine
    inline float FastCosLerp(float angle) const
    {
        const float TWO_PI = 2.0f * 3.14159265358979323846f;
        
        angle = fmodf(angle, TWO_PI);
        if(angle < 0) angle += TWO_PI;
        
        float pos = (angle * TABLE_SIZE) / TWO_PI;
        int idx0 = (int)pos & TABLE_MASK;
        int idx1 = (idx0 + 1) & TABLE_MASK;
        float t = pos - (int)pos;
        
        return m_CosTable[idx0] * (1.0f - t) + m_CosTable[idx1] * t;
    }
    
    // Get table size for statistics
    int GetTableSize() const { return TABLE_SIZE; }
    
    // Get memory usage in bytes
    size_t GetMemoryUsage() const
    {
        return sizeof(m_SinTable) + sizeof(m_CosTable) + 
               sizeof(m_TanTable) + sizeof(m_InvSinTable) + 
               sizeof(m_InvCosTable);
    }
};

// Global instance pointer
CTrigLUT* CTrigLUT::s_pInstance = nullptr;

// Convenience macros for easy access
#define TRIG_LUT CTrigLUT::Get()
#define FAST_SIN(angle) TRIG_LUT.FastSin(angle)
#define FAST_COS(angle) TRIG_LUT.FastCos(angle)
#define FAST_TAN(angle) TRIG_LUT.FastTan(angle)
#define FAST_SINCOS(angle, sin_out, cos_out) TRIG_LUT.FastSinCos(angle, sin_out, cos_out)
#define FAST_SIN_LERP(angle) TRIG_LUT.FastSinLerp(angle)
#define FAST_COS_LERP(angle) TRIG_LUT.FastCosLerp(angle)

#endif // BASE_TL_LOOKUP_TABLE_H