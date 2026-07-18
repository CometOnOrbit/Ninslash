#ifndef GAME_SERVER_ENTITIES_WEAPON_BEHAVIOR_H
#define GAME_SERVER_ENTITIES_WEAPON_BEHAVIOR_H

class CWeapon;

class CWeaponBehaviorExecutor
{
private:
	static bool CreateElectroWall(CWeapon &Weapon);

public:
	static bool Fire(CWeapon &Weapon, float *pKnockback);
	static bool Activate(CWeapon &Weapon);
	static bool Charge(CWeapon &Weapon);
	static bool ReleaseCharge(CWeapon &Weapon, float *pKnockback);
	static bool Throw(CWeapon &Weapon);
	static void CreateProjectile(CWeapon &Weapon);
	static void Trigger(CWeapon &Weapon);
	static void SelfDestruct(CWeapon &Weapon);
};

#endif
