#include "dhdtPapyrusFunctions.h"

#define PAPY_FCN(a) (#a),(PAPYRUS_CLASS_NAME),a

bool RegisterFuncs(VMClassRegistry* registry)
{
	using namespace hdt::papyrus;

	registry->RegisterFunction(
		new NativeFunction5	<StaticFunctionTag, bool, Actor*, TESObjectARMA*, BSFixedString, bool, bool>(PAPY_FCN(ReloadPhysicsFile), registry));

	registry->RegisterFunction(
		new NativeFunction5	<StaticFunctionTag, bool, Actor*, BSFixedString, BSFixedString, bool, bool>(PAPY_FCN(SwapPhysicsFile), registry));

	registry->RegisterFunction(
		new NativeFunction3	<StaticFunctionTag, BSFixedString, Actor*, TESObjectARMA*, bool>(PAPY_FCN(QueryCurrentPhysicsFile), registry));

	return true;
}

bool hdt::papyrus::RegisterAllFunctions(SKSEPapyrusInterface* a_papy_intfc)
{
	return a_papy_intfc->Register(RegisterFuncs);
}

//Some private/protected members are changed to public so that these functions can access them externally.
bool hdt::papyrus::ReloadPhysicsFile(StaticFunctionTag* base, Actor* on_actor, TESObjectARMA* on_item, BSFixedString physics_file_path, bool persist, bool verbose_log)
{
	if (!(on_actor && on_item)) {
		if (verbose_log)
			Console_Print("[DynamicHDT] -- Couldn't parse parameters: on_actor(ptr: %016X), on_item(ptr: %016X).", reinterpret_cast<UInt64>(on_actor), reinterpret_cast<UInt64>(on_item));
		return false;
	}

	const auto& AM = hdt::ActorManager::instance();

	auto& skeletons = AM->getSkeletons();

	bool character_found = false, armor_addon_found = false, succeeded = false;

	std::string old_physics_file_path;

	for (auto& skeleton : skeletons) {
		if (succeeded) break;
		if (!skeleton.skeleton)continue;

		auto owner = skeleton.skeleton->m_owner;

		if (owner && owner->formID == on_actor->formID){
			character_found = true;

			auto& armors = skeleton.getArmors();

			for (auto& armor : armors) {
				if (succeeded) break;
				if (!armor.armorWorn)continue;

				std::string armorName(armor.armorWorn->m_name);

				char buffer[16];
				sprintf_s(buffer, "%08X", on_item->formID);

				if (armorName.find(buffer) != std::string::npos) {
					armor_addon_found = true;
					//Force replacing and reloading. This could lead to assess violation
					try {
						if (armor.physicsFile.first == std::string(physics_file_path)) {
							if (verbose_log)Console_Print("[DynamicHDT] -- Physics file paths are identical, skipping replacing.");
							succeeded = true;
							continue;
						}
						old_physics_file_path = armor.physicsFile.first;
						armor.physicsFile.first = std::string(physics_file_path);
					}
					catch (std::exception& e) {

						Console_Print("[DynamicHDT] ERROR! -- Replacing physics file for ArmorAddon (%08X) on Character (%08X) failed.", on_item->formID, on_actor->formID);

						Console_Print("[DynamicHDT] Error(): %s\nWhat():\n\t%s", typeid(e).name(), e.what());

						return false;
					}

					std::unordered_map<IDStr, IDStr> renameMap = armor.renameMap;

					hdt::Ref<SkyrimSystem> system;

					SkyrimPhysicsWorld::get()->suspendSimulationUntilFinished([&]() {

						system = SkyrimSystemCreator().updateSystem(armor.m_physics, skeleton.npc, armor.armorWorn, armor.physicsFile, std::move(renameMap));

						system->block_resetting = true;

						util::transferCurrentPosesBetweenSystems(armor.m_physics, system);

						armor.setPhysics(system, true);

						system->block_resetting = false;

						}
					);

					if (verbose_log)Console_Print("[DynamicHDT] -- Physics file path switched, now is: \"%s\".", armor.physicsFile.first.c_str());

					succeeded = true;
				}
			}
		}
	}

	//Push into global override data
	if (persist) {
		auto OM = Override::OverrideManager::GetSingleton();
		OM->registerOverride(on_actor->formID, old_physics_file_path, std::string(physics_file_path));
	}

	if (verbose_log)
		Console_Print(
			"[DynamicHDT] -- Character (%08X) %s, ArmorAddon (%08X) %s.",
			on_actor->formID,
			character_found ? "found" : "not found",
			on_item->formID,
			armor_addon_found ? "found" : "not found"
		);

	if (verbose_log && succeeded)
		Console_Print(
			"[DynamicHDT] -- ReloadPhysicsFile() succeeded."
		);

	return succeeded;
}

bool hdt::papyrus::SwapPhysicsFile(StaticFunctionTag* base, Actor* on_actor, BSFixedString old_physics_file_path, BSFixedString new_physics_file_path, bool persist, bool verbose_log)
{
	if (!on_actor) {
		if (verbose_log)Console_Print("[DynamicHDT] -- Couldn't parse parameters: on_actor(ptr: %016X).", reinterpret_cast<UInt64>(on_actor));
		return false;
	}

	const auto& AM = hdt::ActorManager::instance();

	auto& skeletons = AM->getSkeletons();

	bool character_found = false, armor_addon_found = false, succeeded = false;

	for (auto& skeleton : skeletons) {
		if (succeeded) break;
		if (!skeleton.skeleton)continue;

		auto owner = skeleton.skeleton->m_owner;

		if (owner && owner->formID == on_actor->formID) {
			character_found = true;

			auto& armors = skeleton.getArmors();

			for (auto& armor : armors) {
				if (succeeded) break;

				if (armor.physicsFile.first == old_physics_file_path.c_str()) {
					armor_addon_found = true;

					//Force replacing and reloading. This could lead to assess violation
					try {
						if (armor.physicsFile.first == std::string(new_physics_file_path)) {
							if (verbose_log)Console_Print("[DynamicHDT] -- Physics file paths are identical, skipping replacing.");
							succeeded = true;
							continue;
						}
						armor.physicsFile.first = std::string(new_physics_file_path);
					}
					catch (std::exception& e) {

						std::string armorName(armor.armorWorn->m_name);

						UInt32 form_ID = util::splitArmorAddonFormID(armorName);

						Console_Print("[DynamicHDT] ERROR! -- Replacing physics file for ArmorAddon (%08X) on Character (%08X) failed.", form_ID, on_actor->formID);

						Console_Print("[DynamicHDT] Error(): %s\nWhat():\n\t%s", typeid(e).name(), e.what());

						return false;
					}

					std::unordered_map<IDStr, IDStr> renameMap = armor.renameMap;

					hdt::Ref<SkyrimSystem> system;

					SkyrimPhysicsWorld::get()->suspendSimulationUntilFinished([&]() {

						system = SkyrimSystemCreator().updateSystem(armor.m_physics, skeleton.npc, armor.armorWorn, armor.physicsFile, std::move(renameMap));

						system->block_resetting = true;

						util::transferCurrentPosesBetweenSystems(armor.m_physics, system);

						armor.setPhysics(system, true);

						system->block_resetting = false;

						}
					);


					if (verbose_log)Console_Print("[DynamicHDT] -- Physics file path switched, now is: \"%s\".", armor.physicsFile.first.c_str());

					succeeded = true;
				}
			}
		}
	}

	if (persist) {
		auto OM = Override::OverrideManager::GetSingleton();
		OM->registerOverride(on_actor->formID, old_physics_file_path.c_str(), new_physics_file_path.c_str());
	}

	if (verbose_log)
		Console_Print(
			"[DynamicHDT] -- Character (%08X) %s, Physics file path %s.",
			on_actor->formID,
			character_found ? "found" : "not found",
			armor_addon_found ? "found" : "not found"
		);

	if (verbose_log && succeeded)
		Console_Print(
			"[DynamicHDT] -- SwapPhysicsFile() succeeded."
		);

	return succeeded;
}

BSFixedString hdt::papyrus::QueryCurrentPhysicsFile(StaticFunctionTag* base, Actor* on_actor, TESObjectARMA* on_item, bool verbose_log)
{
	if (!(on_actor && on_item)) {
		if (verbose_log)
			Console_Print("[DynamicHDT] -- Couldn't parse parameters: on_actor(ptr: %016X), on_item(ptr: %016X).", reinterpret_cast<UInt64>(on_actor), reinterpret_cast<UInt64>(on_item));
		return false;
	}

	const auto& AM = hdt::ActorManager::instance();

	auto& skeletons = AM->getSkeletons();

	bool character_found = false, armor_addon_found = false, succeeded = false;

	std::string physics_file_path;

	for (auto& skeleton : skeletons) {
		if (succeeded)break;
		if (!skeleton.skeleton)continue;

		auto owner = skeleton.skeleton->m_owner;

		if (owner && owner->formID == on_actor->formID) {
			character_found = true;

			auto& armors = skeleton.getArmors();

			for (auto& armor : armors) {
				if (succeeded)break;
				if (!armor.armorWorn)continue;

				std::string armorName(armor.armorWorn->m_name);

				char buffer[16];
				sprintf_s(buffer, "%08X", on_item->formID);

				if (armorName.find(buffer) != std::string::npos) {
					armor_addon_found = true;
					physics_file_path = armor.physicsFile.first;
					succeeded = true;
				}
			}
		}
	}

	if (verbose_log)
		Console_Print(
			"[DynamicHDT] -- Character (%08X) %s, ArmorAddon (%08X) %s.",
			on_actor->formID,
			character_found ? "found" : "not found",
			on_item->formID,
			armor_addon_found ? "found" : "not found"
		);

	if (verbose_log && succeeded)
		Console_Print(
			"[DynamicHDT] -- QueryCurrentPhysicsFile() querying successful."
		);

	return physics_file_path.c_str();
}


