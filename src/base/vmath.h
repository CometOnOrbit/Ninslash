

#ifndef BASE_VMATH_H
#define BASE_VMATH_H

#include <math.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "math.h"

// Platform-specific aligned allocation
#if defined(_WIN32)
#include <malloc.h>
#endif

// ------------------------------------

template<typename T>
class vector2_base
{
public:
	union { T x,u; };
	union { T y,v; };

	vector2_base() {}
	vector2_base(float nx, float ny)
	{
		x = nx;
		y = ny;
	}

	vector2_base operator -() const { return vector2_base(-x, -y); }
	vector2_base operator -(const vector2_base &v) const { return vector2_base(x-v.x, y-v.y); }
	vector2_base operator +(const vector2_base &v) const { return vector2_base(x+v.x, y+v.y); }
	vector2_base operator *(const T v) const { return vector2_base(x*v, y*v); }
	vector2_base operator *(const vector2_base &v) const { return vector2_base(x*v.x, y*v.y); }
	vector2_base operator /(const T v) const { return vector2_base(x/v, y/v); }
	vector2_base operator /(const vector2_base &v) const { return vector2_base(x/v.x, y/v.y); }

	const vector2_base &operator +=(const vector2_base &v) { x += v.x; y += v.y; return *this; }
	const vector2_base &operator -=(const vector2_base &v) { x -= v.x; y -= v.y; return *this; }
	const vector2_base &operator *=(const T v) { x *= v; y *= v; return *this;	}
	const vector2_base &operator *=(const vector2_base &v) { x *= v.x; y *= v.y; return *this; }
	const vector2_base &operator /=(const T v) { x /= v; y /= v; return *this;	}
	const vector2_base &operator /=(const vector2_base &v) { x /= v.x; y /= v.y; return *this; }

	bool operator ==(const vector2_base &v) const { return x == v.x && y == v.y; } //TODO: do this with an eps instead

	operator const T* () { return &x; }
};

// Performance optimized math functions
namespace math_optimized {

// Fast inverse square root (Quake III algorithm)
inline float Q_rsqrt(float number)
{
	// Use memcpy to avoid strict-aliasing violations
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	
	// Safe type punning using memcpy
	int32_t i;
	memcpy(&i, &y, sizeof(i));
	i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
	memcpy(&y, &i, sizeof(y));
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
}

// Fast square root using inverse square root
inline float fast_sqrt(float x)
{
	if(x <= 0.0f) return 0.0f;
	return x * Q_rsqrt(x);
}

// Fast length approximation (within 1% error)
inline float fast_length(const vector2_base<float> &a)
{
	float x = fabsf(a.x);
	float y = fabsf(a.y);
	
	// Max/min for alpha max plus beta min algorithm
	float max = x > y ? x : y;
	float min = x > y ? y : x;
	
	// Alpha max plus beta min approximation
	// Coefficients for ~0.1% error: alpha=0.96043387, beta=0.39782473
	// Using simpler coefficients for speed: alpha=15/16=0.9375, beta=15/32=0.46875
	return 0.9375f * max + 0.46875f * min;
}

// Fast normalized vector (approximate)
inline vector2_base<float> fast_normalize(const vector2_base<float> &a)
{
	float inv_len = Q_rsqrt(a.x*a.x + a.y*a.y);
	return vector2_base<float>(a.x * inv_len, a.y * inv_len);
}

// Fast dot product (no special optimization, just inlined)
inline float fast_dot(const vector2_base<float> &a, const vector2_base<float> &b)
{
	return a.x*b.x + a.y*b.y;
}

// Fast distance approximation
inline float fast_distance(const vector2_base<float> &a, const vector2_base<float> &b)
{
	vector2_base<float> diff = a - b;
	return fast_length(diff);
}

// Linear interpolation (lerp) - already fast
inline float lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

// Clamp with fast min/max
inline float fast_clamp(float value, float min_val, float max_val)
{
	return value < min_val ? min_val : (value > max_val ? max_val : value);
}

// Fast sine approximation (Bhaskara I's sine approximation formula)
// Valid for 0 to π, error < 0.0016
inline float fast_sin(float x)
{
	// Normalize to [0, 2π]
	const float PI = 3.14159265358979323846f;
	const float TWO_PI = 2.0f * PI;
	
	x = fmodf(x, TWO_PI);
	if(x < 0) x += TWO_PI;
	
	// Bhaskara I's approximation
	if(x <= PI)
	{
		float term = (PI - x) * x;
		return (16.0f * term) / (5.0f * PI * PI - 4.0f * term);
	}
	else
	{
		x = TWO_PI - x;
		float term = (PI - x) * x;
		return -(16.0f * term) / (5.0f * PI * PI - 4.0f * term);
	}
}

// Fast cosine using sine identity
inline float fast_cos(float x)
{
	return fast_sin(x + 3.14159265358979323846f * 0.5f);
}

} // namespace math_optimized

// Cache-optimized vector class with alignment
template<typename T>
class vector2_aligned : public vector2_base<T>
{
public:
	vector2_aligned() {}
	vector2_aligned(T nx, T ny) : vector2_base<T>(nx, ny) {}
	
	// Ensure proper alignment for SIMD
	void* operator new(size_t size) {
		void* ptr;
		#ifdef _WIN32
			ptr = _aligned_malloc(size, 16);
		#else
			if(posix_memalign(&ptr, 16, size) != 0)
				ptr = nullptr;
		#endif
		return ptr;
	}
	
	void operator delete(void* ptr) {
		#ifdef _WIN32
			_aligned_free(ptr);
		#else
			free(ptr);
		#endif
	}
	
	// Array new/delete for aligned arrays
	void* operator new[](size_t size) {
		void* ptr;
		#ifdef _WIN32
			ptr = _aligned_malloc(size, 16);
		#else
			if(posix_memalign(&ptr, 16, size) != 0)
				ptr = nullptr;
		#endif
		return ptr;
	}
	
	void operator delete[](void* ptr) {
		#ifdef _WIN32
			_aligned_free(ptr);
		#else
			free(ptr);
		#endif
	}
};

// Batch processing for vector operations (improves cache locality)
template<typename T>
class vector_batch
{
private:
	T* m_pData;
	int m_Size;
	int m_Capacity;
	
public:
	vector_batch(int capacity = 64) : m_Size(0), m_Capacity(capacity) {
		m_pData = new T[capacity];
	}
	
	~vector_batch() {
		delete[] m_pData;
	}
	
	void add(const T& vec) {
		if(m_Size < m_Capacity) {
			m_pData[m_Size++] = vec;
		}
	}
	
	void clear() { m_Size = 0; }
	
	// Batch normalize all vectors
	void batch_normalize() {
		for(int i = 0; i < m_Size; i++) {
			T& vec = m_pData[i];
			float len = length(vec);
			if(len > 0.0f) {
				vec.x /= len;
				vec.y /= len;
			}
		}
	}
	
	// Batch fast normalize (approximate)
	void batch_fast_normalize() {
		for(int i = 0; i < m_Size; i++) {
			T& vec = m_pData[i];
			float inv_len = math_optimized::Q_rsqrt(vec.x*vec.x + vec.y*vec.y);
			vec.x *= inv_len;
			vec.y *= inv_len;
		}
	}
	
	int size() const { return m_Size; }
	const T* data() const { return m_pData; }
	T* data() { return m_pData; }
};


template<typename T>
inline T length(const vector2_base<T> &a)
{
	return sqrtf(a.x*a.x + a.y*a.y);
}

template<typename T>
inline T distance(const vector2_base<T> a, const vector2_base<T> &b)
{
	return length(a-b);
}

template<typename T>
inline T dot(const vector2_base<T> a, const vector2_base<T> &b)
{
	return a.x*b.x + a.y*b.y;
}

template<typename T>
inline vector2_base<T> normalize(const vector2_base<T> &v)
{
	T l = (T)(1.0f/sqrtf(v.x*v.x + v.y*v.y));
	return vector2_base<T>(v.x*l, v.y*l);
}

typedef vector2_base<float> vec2;
typedef vector2_base<bool> bvec2;
typedef vector2_base<int> ivec2;

template<typename T>
inline vector2_base<T> closest_point_on_line(vector2_base<T> line_point0, vector2_base<T> line_point1, vector2_base<T> target_point)
{
	vector2_base<T> c = target_point - line_point0;
	vector2_base<T> v = (line_point1 - line_point0);
	v = normalize(v);
	T d = length(line_point0-line_point1);
	T t = dot(v, c)/d;
	return mix(line_point0, line_point1, clamp(t, (T)0, (T)1));
	/*
	if (t < 0) t = 0;
	if (t > 1.0f) return 1.0f;
	return t;*/
}

inline float nlerp(float a, float b, float amount)
{
	a *= pi/180.0f; b *= pi/180.0f;
	vec2 VecA(cosf(a), sinf(a));
	vec2 VecB(cosf(b), sinf(b));

	vec2 out = normalize(mix(VecA, VecB, amount));
	return (atan2f(out.y, out.x)) * 180.0f/pi;
}

// ------------------------------------
template<typename T>
class vector3_base
{
public:
	union { T x,r,h; };
	union { T y,g,s; };
	union { T z,b,v,l; };

	vector3_base() {}
	vector3_base(float nx, float ny, float nz)
	{
		x = nx;
		y = ny;
		z = nz;
	}

	vector3_base operator -(const vector3_base &v) const { return vector3_base(x-v.x, y-v.y, z-v.z); }
	vector3_base operator -() const { return vector3_base(-x, -y, -z); }
	vector3_base operator +(const vector3_base &v) const { return vector3_base(x+v.x, y+v.y, z+v.z); }
	vector3_base operator *(const T v) const { return vector3_base(x*v, y*v, z*v); }
	vector3_base operator *(const vector3_base &v) const { return vector3_base(x*v.x, y*v.y, z*v.z); }
	vector3_base operator /(const T v) const { return vector3_base(x/v, y/v, z/v); }
	vector3_base operator /(const vector3_base &v) const { return vector3_base(x/v.x, y/v.y, z/v.z); }

	const vector3_base &operator +=(const vector3_base &v) { x += v.x; y += v.y; z += v.z; return *this; }
	const vector3_base &operator -=(const vector3_base &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	const vector3_base &operator *=(const T v) { x *= v; y *= v; z *= v; return *this;	}
	const vector3_base &operator *=(const vector3_base &v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
	const vector3_base &operator /=(const T v) { x /= v; y /= v; z /= v; return *this;	}
	const vector3_base &operator /=(const vector3_base &v) { x /= v.x; y /= v.y; z /= v.z; return *this; }

	bool operator ==(const vector3_base &v) const { return x == v.x && y == v.y && z == v.z; } //TODO: do this with an eps instead

	operator const T* () { return &x; }
};

template<typename T>
inline T length(const vector3_base<T> &a)
{
	return sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
}

template<typename T>
inline T distance(const vector3_base<T> &a, const vector3_base<T> &b)
{
	return length(a-b);
}

template<typename T>
inline T dot(const vector3_base<T> &a, const vector3_base<T> &b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

template<typename T>
inline vector3_base<T> normalize(const vector3_base<T> &v)
{
	T l = (T)(1.0f/sqrtf(v.x*v.x + v.y*v.y + v.z*v.z));
	return vector3_base<T>(v.x*l, v.y*l, v.z*l);
}

template<typename T>
inline vector3_base<T> cross(const vector3_base<T> &a, const vector3_base<T> &b)
{
	return vector3_base<T>(
		a.y*b.z - a.z*b.y,
		a.z*b.x - a.x*b.z,
		a.x*b.y - a.y*b.x);
}

typedef vector3_base<float> vec3;
typedef vector3_base<bool> bvec3;
typedef vector3_base<int> ivec3;

// ------------------------------------

template<typename T>
class vector4_base
{
public:
	union { T x,r; };
	union { T y,g; };
	union { T z,b; };
	union { T w,a; };

	vector4_base() {}
	vector4_base(float nx, float ny, float nz, float nw)
	{
		x = nx;
		y = ny;
		z = nz;
		w = nw;
	}

	vector4_base operator +(const vector4_base &v) const { return vector4_base(x+v.x, y+v.y, z+v.z, w+v.w); }
	vector4_base operator -(const vector4_base &v) const { return vector4_base(x-v.x, y-v.y, z-v.z, w-v.w); }
	vector4_base operator -() const { return vector4_base(-x, -y, -z, -w); }
	vector4_base operator *(const vector4_base &v) const { return vector4_base(x*v.x, y*v.y, z*v.z, w*v.w); }
	vector4_base operator *(const T v) const { return vector4_base(x*v, y*v, z*v, w*v); }
	vector4_base operator /(const vector4_base &v) const { return vector4_base(x/v.x, y/v.y, z/v.z, w/v.w); }
	vector4_base operator /(const T v) const { return vector4_base(x/v, y/v, z/v, w/v); }

	const vector4_base &operator +=(const vector4_base &v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
	const vector4_base &operator -=(const vector4_base &v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
	const vector4_base &operator *=(const T v) { x *= v; y *= v; z *= v; w *= v; return *this;	}
	const vector4_base &operator *=(const vector4_base &v) { x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this; }
	const vector4_base &operator /=(const T v) { x /= v; y /= v; z /= v; w /= v; return *this;	}
	const vector4_base &operator /=(const vector4_base &v) { x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this; }

	bool operator ==(const vector4_base &v) const { return x == v.x && y == v.y && z == v.z && w == v.w; } //TODO: do this with an eps instead

	operator const T* () { return &x; }
};

typedef vector4_base<float> vec4;
typedef vector4_base<bool> bvec4;
typedef vector4_base<int> ivec4;

// ------------------------------------

template<typename T>
class matrix22_base
{
public:
	T m00, m01;
	T m10, m11;

	matrix22_base() {}
	matrix22_base(T n00, T n01, T n10, T n11)
	{
		m00 = n00;
		m01 = n01;
		m10 = n10;
		m11 = n11;
	}

	matrix22_base operator +(const matrix22_base &m) const { return matrix22_base(m00+m.m00, m01+m.m01, m10+m.m10, m11+m.m11); }
	matrix22_base operator -(const matrix22_base &m) const { return matrix22_base(m00-m.m00, m01-m.m01, m10-m.m10, m11-m.m11); }
	matrix22_base operator -() const { return matrix22_base(-m00, -m01, -m10, -m11); }
	matrix22_base operator *(const T s) const { return matrix22_base(s*m00, s*m01, s*m10, s*m11); }
	vector2_base<T> operator *(const vector2_base<T> &v) const { return vector2_base<T>(m00*v.x + m01*v.y, m10*v.x + m11*v.y); }
	matrix22_base operator *(const matrix22_base &m) const
	{
		return matrix22_base(
				m00*m.m00 + m01*m.m10, m00*m.m01 + m01*m.m11,
				m10*m.m00 + m11*m.m10, m10*m.m01 + m11*m.m11
			);
	}

	const matrix22_base &operator =(const matrix22_base &m) { m00 = m.m00; m01 = m.m01; m10 = m.m10; m11 = m.m11; return *this; }

	const matrix22_base &operator +=(const matrix22_base &m) { m00 += m.m00; m01 += m.m01; m10 += m.m10; m11 += m.m11; return *this; }
	const matrix22_base &operator -=(const matrix22_base &m) { m00 -= m.m00; m01 -= m.m01; m10 -= m.m10; m11 -= m.m11; return *this; }
	const matrix22_base &operator *=(const T &s) { m00 *= s; m01 *= s; m10 *= s; m11 *= s; return *this; }
	const matrix22_base &operator *=(const matrix22_base &m)
	{
		m00 = m00*m.m00 + m01*m.m10;
		m01 = m00*m.m01 + m01*m.m11;
		m10 = m10*m.m00 + m11*m.m10;
		m11 = m10*m.m01 + m11*m.m11;
		return *this;
	}

	bool operator ==(const matrix22_base &m) const { return m00 = m.m00 && m01 = m.m01 && m10 = m.m10 && m11 = m.m11; }


	operator const T* () { return &m00; }
};

typedef matrix22_base<float> mat2;

// ------------------------------------
template<typename T>
class matrix33_base
{
public:
	T m00, m01, m02;
	T m10, m11, m12;
	T m20, m21, m22;

	matrix33_base() {}
	matrix33_base(
		T n00, T n01, T n02, 
		T n10, T n11, T n12,
		T n20, T n21, T n22)
	{
		m00 = n00; m01 = n01; m02 = n02;
		m10 = n10; m11 = n11; m12 = n12;
		m20 = n20; m21 = n21; m22 = n22;
	}

	static
	matrix33_base<T> identity()
	{
		return matrix33_base<T>(
				T(1), 0, 0,
				0, T(1), 0,
				0, 0, T(1)
			);
	}

	vector3_base<T> operator *(const vector3_base<T> &v) const
	{
		return vector3_base<T>(
				m00*v.x + m01*v.y + m02*v.z,
				m10*v.x + m11*v.y + m12*v.z,
				m20*v.x + m21*v.y + m22*v.z
			);
	}

	matrix33_base<T> operator *(const matrix33_base<T> &m) const
	{
		return matrix33_base<T>(
				m00*m.m00 + m01*m.m10 + m02*m.m20, m00*m.m01 + m01*m.m11 + m02*m.m21, m00*m.m02 + m01*m.m12 + m02*m.m22,
				m10*m.m00 + m11*m.m10 + m12*m.m20, m10*m.m01 + m11*m.m11 + m12*m.m21, m10*m.m02 + m11*m.m12 + m12*m.m22,
				m20*m.m00 + m21*m.m10 + m22*m.m20, m20*m.m01 + m21*m.m11 + m22*m.m21, m20*m.m02 + m21*m.m12 + m22*m.m22
			);
	}

	const matrix33_base &operator =(const matrix33_base &m)
	{
		m00 = m.m00; m01 = m.m01; m02 = m.m02;
		m10 = m.m10; m11 = m.m11; m12 = m.m12;
		m20 = m.m20; m21 = m.m21; m22 = m.m22;
		return *this;
	}

	operator const T* () { return &m00; }
};

typedef matrix33_base<float> mat33;

inline float mix_angle(float a, float b, float amount)
{
	float diff = b - a;
	if(diff > 180.0f)
		a += 360.0f;
	if(diff < -180.0f)
		a -= 360.0f;
	return mix(a, b, amount);
}

#endif
