
#include <engine/shared/config.h>

#include <base/math.h>
#include <game/collision.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>

#include <game/client/customstuff.h>

#include "camera.h"
#include "controls.h"

CCamera::CCamera()
{
	m_CamType = CAMTYPE_UNDEFINED;
	m_LastUpdate = 0;
	m_CameraSmoothStart = 0;
	m_Zoom = 1.0f;
	m_TargetZoom = 1.0f;
	m_Center = vec2(0, 0);
	m_Center2 = vec2(0, 0);
	m_PrevCenter = vec2(0, 0);
	m_TargetCenter = vec2(0, 0);
}

void CCamera::OnRender()
{
	// Gameplay zoom is server-authoritative. Only a server-granted spectator
	// may use the local preference; all other clients follow the snapshotted
	// value so a player cannot change their view independently.
	const float ZoomValue = m_pClient->LocalZoomAllowed() ?
		(float)g_Config.m_ClZoom : (float)m_pClient->ServerZoom();
	m_TargetZoom = ZoomValue / 10.0f;
	if(m_TargetZoom < 0.1f)
		m_TargetZoom = 0.1f;

	// Smooth zoom toward target every frame.
	m_Zoom += (m_TargetZoom - m_Zoom) * clamp(Client()->RenderFrameTime() * 8.0f, 0.0f, 1.0f);

	// update camera center
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
	{
		if(m_CamType != CAMTYPE_SPEC)
		{
			m_pClient->m_pControls->m_MousePos = m_PrevCenter;
			m_pClient->m_pControls->ClampMousePos();
			m_CamType = CAMTYPE_SPEC;
			m_CameraSmoothStart = time_get();
		}

		m_Center2 = m_pClient->m_pControls->m_MousePos;
		m_TargetCenter = m_Center2;

		int64 currentTime = time_get();
		if((currentTime - m_LastUpdate > time_freq()) || (m_LastUpdate == 0))
			m_LastUpdate = currentTime;

		if(g_Config.m_ClSmoothSpectatingTime > 0)
		{
			float SmoothTime = g_Config.m_ClSmoothSpectatingTime / 1000.0f;
			float FrameTime = Client()->RenderFrameTime();
			if(SmoothTime > 0.0001f)
			{
				float t = clamp(FrameTime / SmoothTime, 0.0f, 1.0f);
				float s = t * t * (3.0f - 2.0f * t);
				m_Center.x = m_PrevCenter.x + (m_Center2.x - m_PrevCenter.x) * s;
				m_Center.y = m_PrevCenter.y + (m_Center2.y - m_PrevCenter.y) * s;
			}
		}
		else
		{
			int step = time_freq() / 800;
			if(step <= 0)
				step = 1;

			int i = 0;
			for(; m_LastUpdate < currentTime; m_LastUpdate += step)
			{
				m_Center.x += (m_Center2.x - m_Center.x) / float(50);
				m_Center.y += (m_Center2.y - m_Center.y) / float(50);
				if(i++ > 20)
					break;
			}
		}

		CustomStuff()->m_LocalTeam = TEAM_SPECTATORS;
	}
	else
	{
		if(m_CamType != CAMTYPE_PLAYER)
		{
			m_pClient->m_pControls->ClampMousePos();
			m_CamType = CAMTYPE_PLAYER;
			m_CameraSmoothStart = time_get();
		}

		vec2 CameraOffset(0, 0);
		float l = length(m_pClient->m_pControls->m_MousePos);
		if(l > 0.0001f)
		{
			float DeadZone = g_Config.m_ClMouseDeadzone;
			float FollowFactor = g_Config.m_ClMouseFollowfactor / 100.0f;
			float OffsetAmount = max(l - DeadZone, 0.0f) * FollowFactor;
			float MaxDist = (float)g_Config.m_ClMouseMaxDistance;
			if(OffsetAmount > MaxDist)
				OffsetAmount = MaxDist;
			CameraOffset = normalize(m_pClient->m_pControls->m_MousePos) * OffsetAmount;
		}

		vec2 TargetCenter;
		if(m_pClient->m_Snap.m_SpecInfo.m_Active)
			TargetCenter = m_pClient->m_Snap.m_SpecInfo.m_Position + CameraOffset;
		else
		{
			TargetCenter = m_pClient->m_LocalCharacterPos + CameraOffset;
			TargetCenter += CustomStuff()->CameraOffset(Client()->RenderFrameTime());
		}

		m_TargetCenter = TargetCenter;

		if(g_Config.m_ClDyncamSmoothness > 0 || g_Config.m_ClDyncamStabilizing > 0)
		{
			float FrameTime = Client()->RenderFrameTime();
			float SmoothFactor = 1.0f - g_Config.m_ClDyncamSmoothness / 100.0f;
			if(SmoothFactor < 0.001f)
				SmoothFactor = 0.001f;

			if(g_Config.m_ClDyncamStabilizing > 0)
			{
				float MouseSpeed = length(m_pClient->m_pControls->m_MousePos) * FrameTime;
				float StabilizeFactor =
					1.0f - g_Config.m_ClDyncamStabilizing / 100.0f * clamp(MouseSpeed / 10.0f, 0.0f, 1.0f);
				SmoothFactor *= StabilizeFactor;
			}

			float Step = clamp(SmoothFactor * FrameTime * 10.0f, 0.0f, 1.0f);
			m_Center.x = m_PrevCenter.x + (TargetCenter.x - m_PrevCenter.x) * Step;
			m_Center.y = m_PrevCenter.y + (TargetCenter.y - m_PrevCenter.y) * Step;
		}
		else
		{
			m_Center = TargetCenter;
		}
	}

	m_PrevCenter = m_Center;

	Graphics()->CameraToShaders(Graphics()->ScreenWidth(), Graphics()->ScreenHeight(), m_Center.x, m_Center.y);
}
