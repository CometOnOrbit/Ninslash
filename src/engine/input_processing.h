#ifndef ENGINE_INPUT_PROCESSING_H
#define ENGINE_INPUT_PROCESSING_H

#include <base/math.h>
#include <base/vmath.h>

#include <cmath>

struct CProcessedStick
{
	vec2 m_Value;
	float m_Magnitude;
};

inline CProcessedStick ProcessRadialStick(vec2 Raw, float InnerDeadzone, float OuterDeadzone, float Curve)
{
	const float Magnitude = length(Raw);
	const float Inner = clamp(InnerDeadzone, 0.0f, 0.95f);
	const float Outer = clamp(OuterDeadzone, Inner + 0.01f, 1.0f);
	if(Magnitude <= Inner)
		return {vec2(0.0f, 0.0f), 0.0f};
	const float Remapped = powf(clamp((Magnitude - Inner) / (Outer - Inner), 0.0f, 1.0f), max(0.1f, Curve));
	return {normalize(Raw) * Remapped, Remapped};
}

inline int ProcessDigitalAxis(float Value, int Previous, float PressThreshold, float ReleaseThreshold)
{
	const float Press = clamp(PressThreshold, 0.05f, 1.0f);
	const float Release = clamp(ReleaseThreshold, 0.0f, Press);
	if(Previous < 0 && Value <= -Release)
		return -1;
	if(Previous > 0 && Value >= Release)
		return 1;
	if(Value <= -Press)
		return -1;
	if(Value >= Press)
		return 1;
	return 0;
}

inline bool ProcessAnalogButton(float Value, bool Previous, float PressThreshold, float ReleaseThreshold)
{
	return Previous ? Value >= ReleaseThreshold : Value >= PressThreshold;
}

inline vec2 IntegrateAimStick(vec2 Processed, float UnitsPerSecond, float Sensitivity, float DeltaSeconds)
{
	return Processed * UnitsPerSecond * Sensitivity * clamp(DeltaSeconds, 0.0f, 0.05f);
}

#endif
