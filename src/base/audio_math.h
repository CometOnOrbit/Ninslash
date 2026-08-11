// Audio math utilities from https://gitlab.com/obani/sound_exp
// Pure algorithm layer — no engine / game dependencies
#ifndef BASE_AUDIO_MATH_H
#define BASE_AUDIO_MATH_H

class IFloat
{
	float m_Value = 0.0f;
	float m_Increment = 0.0f;

public:
	IFloat() = default;
	explicit IFloat(float value) : m_Value(value) {}

	float next() { return m_Value += m_Increment; }

	IFloat &set(float target, int steps)
	{
		m_Increment = (target - m_Value) * (1.0f / float(steps));
		return *this;
	}

	void jump(float value)
	{
		m_Value = value;
		m_Increment = 0.0f;
	}

	float get() const { return m_Value; }
};

// https://gitlab.com/obani/sound_exp/-/blob/main/util/ifloat.h
struct NextFloat
{
	float v = 0.0f;
	NextFloat() = default;
	constexpr NextFloat(float val) : v(val) {}
	constexpr float next() const { return v; }
};

enum class CrossfadeType
{
	Linear,
	Smooth,
};

template<CrossfadeType TYPE>
constexpr float Crossfade(float a, float b, float x)
{
	if constexpr(TYPE == CrossfadeType::Linear)
		return a + (b - a) * x;
	else
		return a + (b - a) * (3.0f * x * x - 2.0f * x * x * x);
}

// https://gitlab.com/obani/sound_exp/-/blob/main/audio/filters.h
class LowPass1
{
	float m_State = 0.0f;

public:
	float step(float sig, float coeff)
	{
		return m_State += (sig - m_State) * coeff;
	}
	float get() const { return m_State; }
};

// https://gitlab.com/obani/sound_exp/-/blob/main/audio/filters.h
class HighPass1
{
	LowPass1 m_LP;

public:
	float step(float sig, float coeff)
	{
		return sig - m_LP.step(sig, coeff);
	}
};

// Asymmetric attack/release smoothing — fast rise, slow decay
class AttackRelease
{
	float m_State = 0.0f;

public:
	float step(float sig, float attack, float release)
	{
		float err = sig - m_State;
		float coeff = err > 0.0f ? attack : release;
		return m_State += err * coeff;
	}
	float get() const { return m_State; }
	void jump(float v) { m_State = v; }
};

#endif // BASE_AUDIO_MATH_H
