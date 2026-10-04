#include <base/system.h>
#include <game/client/spine.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

static bool Check(bool Condition, const char *pMessage)
{
	if(!Condition)
		fprintf(stderr, "%s\n", pMessage);
	return Condition;
}

int main(int argc, char **argv)
{
	const std::string Directory = argc > 1 ? argv[1] : "data/anim/foundry_warden";
	const std::string Stem = argc > 2 ? argv[2] : "warden";
	std::ifstream JsonFile(Directory + "/" + Stem + ".json"), AtlasFile(Directory + "/" + Stem + ".atlas");
	const std::string Json((std::istreambuf_iterator<char>(JsonFile)), {});
	const std::string AtlasText((std::istreambuf_iterator<char>(AtlasFile)), {});
	CSpineReader Reader;
	array<CSpineBone> Bones;
	array<CSpineSlot> Slots;
	SkinMap Skins;
	std::map<string, CSpineAnimation> Animations;
	CSpineAtlas Atlas;
	if(!Check(Reader.Load(Json.c_str(), &Bones, &Slots, &Skins, &Animations), "Native JSON reader failed") ||
	   !Check(Reader.LoadAtlas(AtlasText.c_str(), &Atlas), "Native atlas reader failed"))
		return 1;
	if(!Check(Bones.size() > 0 && Slots.size() > 0 && Animations.size() == 9, "Missing rig or animation data") ||
	   !Check(Atlas.m_lPages.size() == 1 && Atlas.m_lPages[0].m_lRegions.size() > 0, "Unexpected atlas layout"))
		return 1;
	std::set<std::string> BoneNames, RegionNames;
	for(int i = 0; i < Bones.size(); ++i)
	{
		const CSpineBone &Bone = Bones[i];
		if(!Check(i == 0 || BoneNames.count(Bone.m_Parent.cstr()), "Parent must precede child") ||
		   !Check(std::isfinite(Bone.m_Rotation) && std::isfinite(Bone.m_X) && std::isfinite(Bone.m_Y), "Invalid transform"))
			return 1;
		BoneNames.insert(Bone.m_Name.cstr());
	}
	const CSpineAtlasPage &Page = Atlas.m_lPages[0];
	std::ifstream Texture(std::string("data/anim/") + Page.m_Name.cstr(), std::ios::binary);
	if(!Check(Texture.good(), "Texture missing at the engine's data/anim-relative page path"))
		return 1;
	for(int i = 0; i < Page.m_lRegions.size(); ++i)
	{
		const CSpineAtlasRegion &Region = Page.m_lRegions[i];
		if(!Check(Region.m_X >= 0 && Region.m_Y >= 0 && Region.m_X + Region.m_Width <= Page.m_Width &&
			Region.m_Y + Region.m_Height <= Page.m_Height, "Region outside atlas"))
			return 1;
		RegionNames.insert(Region.m_Name.cstr());
	}
	for(int i = 0; i < Slots.size(); ++i)
	{
		const CSpineSlot &Slot = Slots[i];
		if(!Check(BoneNames.count(Slot.m_Bone.cstr()) && RegionNames.count(Slot.m_Attachment.cstr()), "Unresolved slot"))
			return 1;
		const CSpineAttachment &Attachment = Skins["default"][Slot.m_Name][Slot.m_Attachment];
		if(!Check(Attachment.m_Type == SPINE_ATTACHMENT_REGION && Attachment.m_Region.m_Width > 0 &&
			Attachment.m_Region.m_Height > 0, "Unsupported or empty attachment"))
			return 1;
	}
	for(const auto &Animation : Animations)
		for(const auto &Timeline : Animation.second.m_lBoneTimeline)
			if(!Check(BoneNames.count(Timeline.first.cstr()), "Animation references missing bone"))
				return 1;
	printf("Native Ninslash Spine reader: PASS (%d bones, %d slots, %d regions, %d animations)\n",
		Bones.size(), Slots.size(), Page.m_lRegions.size(), (int)Animations.size());
	return 0;
}
