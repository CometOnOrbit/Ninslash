// https://gitlab.com/obani/sound_exp/-/blob/main/util/ifloat.h
#pragma once

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
