#include <engine/input_processing.h>
#include <engine/input.h>
#include <game/input_buffer.h>

#include <cassert>
#include <cmath>

int main()
{
	const CProcessedStick Center = ProcessRadialStick(vec2(0.1f, 0.0f), 0.18f, 0.95f, 1.5f);
	assert(Center.m_Magnitude == 0.0f);
	const CProcessedStick Edge = ProcessRadialStick(vec2(1.0f, 0.0f), 0.18f, 0.95f, 1.5f);
	assert(fabsf(Edge.m_Value.x - 1.0f) < 0.0001f);
	assert(ProcessDigitalAxis(0.36f, 0, 0.35f, 0.25f) == 1);
	assert(ProcessDigitalAxis(0.30f, 1, 0.35f, 0.25f) == 1);
	assert(ProcessDigitalAxis(0.20f, 1, 0.35f, 0.25f) == 0);
	assert(ProcessAnalogButton(0.13f, false, 0.12f, 0.08f));
	assert(ProcessAnalogButton(0.09f, true, 0.12f, 0.08f));
	assert(!ProcessAnalogButton(0.07f, true, 0.12f, 0.08f));
	assert(QueueInputUntil(100, 3, 3) == 103);
	assert(QueueInputUntil(100, 4, 3) == 0);
	assert(InputBufferState(102, 103, false) == INPUT_BUFFER_WAITING);
	assert(InputBufferState(103, 103, true) == INPUT_BUFFER_READY);
	assert(InputBufferState(104, 103, true) == INPUT_BUFFER_EXPIRED);
	assert(SwitchInputBufferTicks(0, 50) == 3);
	assert(SwitchInputBufferTicks(12, 50) == 15);
	assert(SwitchInputBufferTicks(80, 50) == 50);
	assert(IInput::ShouldGrabMouse(IInput::MOUSE_MODE_WARP_CENTER, true));
	assert(!IInput::ShouldGrabMouse(IInput::MOUSE_MODE_WARP_CENTER, false));
	assert(!IInput::ShouldGrabMouse(IInput::MOUSE_MODE_NONE, true));
	assert(!IInput::ShouldGrabMouse(IInput::MOUSE_MODE_NO_MOUSE, true));

	CDiscreteInputPulse WeaponPulse;
	WeaponPulse.Queue(2);
	assert(WeaponPulse.Prepare() == 2);
	WeaponPulse.OnSent(2);
	// A repeat arriving before the release packet is retained, not lost.
	WeaponPulse.Queue(2);
	assert(WeaponPulse.Prepare() == 0);
	WeaponPulse.OnSent(0);
	assert(WeaponPulse.Prepare() == 2);
	WeaponPulse.OnSent(2);
	assert(WeaponPulse.Prepare() == 0);
	WeaponPulse.OnSent(0);
	// A newer direct key replaces an older unsent key.
	WeaponPulse.Queue(3);
	WeaponPulse.Queue(4);
	assert(WeaponPulse.Prepare() == 4);
	WeaponPulse.OnSent(4);
	assert(WeaponPulse.Prepare() == 0);
	WeaponPulse.OnSent(0);
	// Wheel input cancels an unsent direct selection without swallowing its
	// own independent counter event.
	WeaponPulse.Queue(3);
	WeaponPulse.CancelQueued();
	assert(WeaponPulse.Prepare() == -1);

	vec2 At30(0, 0), At60(0, 0), At144(0, 0), At240(0, 0);
	for(int i = 0; i < 30; i++) At30 += IntegrateAimStick(vec2(0.6f, -0.2f), 1400.0f, 1.0f, 1.0f / 30.0f);
	for(int i = 0; i < 60; i++) At60 += IntegrateAimStick(vec2(0.6f, -0.2f), 1400.0f, 1.0f, 1.0f / 60.0f);
	for(int i = 0; i < 144; i++) At144 += IntegrateAimStick(vec2(0.6f, -0.2f), 1400.0f, 1.0f, 1.0f / 144.0f);
	for(int i = 0; i < 240; i++) At240 += IntegrateAimStick(vec2(0.6f, -0.2f), 1400.0f, 1.0f, 1.0f / 240.0f);
	assert(distance(At30, At60) < 0.1f);
	assert(distance(At60, At144) < 0.1f);
	assert(distance(At144, At240) < 0.1f);
	return 0;
}
