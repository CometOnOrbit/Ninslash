

#ifndef GAME_TUNING_H
#define GAME_TUNING_H
#undef GAME_TUNING_H // this file will be included several times

// physics tuning
MACRO_TUNING_PARAM(ControlSpeed, control_speed, 0.9f)
MACRO_TUNING_PARAM(JumpPower, jump_power, 11.55f)


MACRO_TUNING_PARAM(BallSize, ball_size, 64.0f)

MACRO_TUNING_PARAM(GroundControlSpeed, ground_control_speed, 480.0f / TicksPerSecond)
MACRO_TUNING_PARAM(GroundControlAccel, ground_control_accel, 90.0f / TicksPerSecond)
MACRO_TUNING_PARAM(GroundReverseAccel, ground_reverse_accel, 1.35f)
MACRO_TUNING_PARAM(GroundFriction, ground_friction, 0.42f)
MACRO_TUNING_PARAM(GroundJumpImpulse, ground_jump_impulse, 11.55f)
MACRO_TUNING_PARAM(JumpCoyoteTicks, jump_coyote_ticks, 4.0f)
MACRO_TUNING_PARAM(JumpBufferTicks, jump_buffer_ticks, 4.0f)
MACRO_TUNING_PARAM(HookLength, hook_length, 520.0f)
MACRO_TUNING_PARAM(HookFireSpeed, hook_fire_speed, 75.0f)
MACRO_TUNING_PARAM(HookDragAccel, hook_drag_accel, 2.3f)
MACRO_TUNING_PARAM(HookDragSpeed, hook_drag_speed, 15.5f)
MACRO_TUNING_PARAM(HookDragMinDistFactor, hook_drag_min_dist_factor, 1.35f)
MACRO_TUNING_PARAM(HookMoveAlongFactor, hook_move_along_factor, 1.08f)
MACRO_TUNING_PARAM(HookMoveAgainstFactor, hook_move_against_factor, 0.72f)
MACRO_TUNING_PARAM(HookDownFactor, hook_down_factor, 0.40f)
MACRO_TUNING_PARAM(HookTargetHoldSeconds, hook_target_hold_seconds, 1.35f)
MACRO_TUNING_PARAM(WallrunImpulse, wall_run_impulse, 9.6f)
MACRO_TUNING_PARAM(WallJumpDelayTicks, wall_jump_delay_ticks, 2.0f)
MACRO_TUNING_PARAM(WallJumpHorizontalImpulse, wall_jump_horizontal_impulse, 8.0f)
MACRO_TUNING_PARAM(WallJumpDirectionLockTicks, wall_jump_direction_lock_ticks, 4.0f)
MACRO_TUNING_PARAM(AirControlSpeed, air_control_speed, 475.0f / TicksPerSecond)
MACRO_TUNING_PARAM(AirControlAccel, air_control_accel, 58.0f / TicksPerSecond)
MACRO_TUNING_PARAM(AirReverseAccel, air_reverse_accel, 1.12f)
MACRO_TUNING_PARAM(AirFriction, air_friction, 0.94f)
MACRO_TUNING_PARAM(Gravity, gravity, 0.636f)
MACRO_TUNING_PARAM(JetpackControlSpeed, jetpack_control_speed, 12.2f)
MACRO_TUNING_PARAM(JetpackControlAccel, jetpack_control_accel, 2.1f)

MACRO_TUNING_PARAM(SlideFriction, slide_friction, 0.8f)
MACRO_TUNING_PARAM(SlideSlopeAcceleration, slide_slope_acceleration, 0.3f)
MACRO_TUNING_PARAM(SlopeDeceleration, slope_deceleration, 0.0f)
MACRO_TUNING_PARAM(SlopeAscendingControlSpeed, slope_ascending_control_speed, 10.0f)
MACRO_TUNING_PARAM(SlopeDescendingControlSpeed, slope_descending_control_speed, 10.0f)
MACRO_TUNING_PARAM(SlideControlSpeed, slide_control_speed, 540.0f / TicksPerSecond)
MACRO_TUNING_PARAM(SlideActivationSpeed, slide_activation_speed, 6.2f)
MACRO_TUNING_PARAM(SlideMinimumSpeed, slide_minimum_speed, 6.8f)
MACRO_TUNING_PARAM(RollLandingSpeed, roll_landing_speed, 11.5f)
MACRO_TUNING_PARAM(RollDurationTicks, roll_duration_ticks, 12.0f)
MACRO_TUNING_PARAM(DashPower, dash_power, 22.5f)

MACRO_TUNING_PARAM(VelrampStart, velramp_start, 550)
MACRO_TUNING_PARAM(VelrampRange, velramp_range, 2000)
MACRO_TUNING_PARAM(VelrampCurvature, velramp_curvature, 1.4f)

// weapon tuning
MACRO_TUNING_PARAM(SwordSpeed, sword_speed, 700.0f)

MACRO_TUNING_PARAM(GunCurvature, gun_curvature, 1.25f)
MACRO_TUNING_PARAM(GunSpeed, gun_speed, 3200.0f)
MACRO_TUNING_PARAM(GunLifetime, gun_lifetime, 2.0f)

MACRO_TUNING_PARAM(WalkerCurvature, walker_curvature, 0.3f)
MACRO_TUNING_PARAM(WalkerSpeed, walker_speed, 2200.0f)
MACRO_TUNING_PARAM(WalkerLifetime, walker_lifetime, 2.0f)

MACRO_TUNING_PARAM(StarDroidCurvature, droid_star_curvature, 0.0f)
MACRO_TUNING_PARAM(StarDroidSpeed, droid_star_speed, 20.0f)
MACRO_TUNING_PARAM(StarDroidLifetime, droid_star_lifetime, 2.0f)

MACRO_TUNING_PARAM(ShotgunCurvature, shotgun_curvature, 1.25f)
MACRO_TUNING_PARAM(ShotgunSpeed, shotgun_speed, 2750.0f)
MACRO_TUNING_PARAM(ShotgunSpeeddiff, shotgun_speeddiff, 0.8f)
MACRO_TUNING_PARAM(ShotgunLifetime, shotgun_lifetime, 0.20f)

MACRO_TUNING_PARAM(GrenadeCurvature, grenade_curvature, 7.0f)
MACRO_TUNING_PARAM(GrenadeSpeed, grenade_speed, 1000.0f)
MACRO_TUNING_PARAM(GrenadeLifetime, grenade_lifetime, 2.0f)

MACRO_TUNING_PARAM(ElectricCurvature, electric_curvature, 2.0f)
MACRO_TUNING_PARAM(ElectricSpeed, electric_speed, 1750.0f)
MACRO_TUNING_PARAM(ElectricLifetime, electric_lifetime, 4.0f)

MACRO_TUNING_PARAM(FlamerCurvature, flamer_curvature, 6.0f)
MACRO_TUNING_PARAM(FlamerSpeed, flamer_speed, 1100.0f)
MACRO_TUNING_PARAM(FlamerLifetime, flamer_lifetime, 2.0f)

MACRO_TUNING_PARAM(LaserReach, laser_reach, 800.0f)
MACRO_TUNING_PARAM(LaserBounceDelay, laser_bounce_delay, 150)
MACRO_TUNING_PARAM(LaserBounceNum, laser_bounce_num, 1)
MACRO_TUNING_PARAM(LaserBounceCost, laser_bounce_cost, 0)
MACRO_TUNING_PARAM(LaserDamage, laser_damage, 5)

MACRO_TUNING_PARAM(PlayerCollision, player_collision, 1)
MACRO_TUNING_PARAM(PlayerHooking, player_hooking, 1)
#endif
