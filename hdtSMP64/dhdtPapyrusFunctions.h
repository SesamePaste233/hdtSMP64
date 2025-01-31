#pragma once
#include "DynamicHDT.h"

#include <skse64/skse64/GameReferences.h>

#include <skse64/skse64/PapyrusVM.h>
#include <skse64/skse64/PapyrusNativeFunctions.h>

namespace hdt {
	namespace papyrus {

		bool RegisterAllFunctions(SKSEPapyrusInterface* a_papy_intfc);

		bool ReloadPhysicsFile(StaticFunctionTag *base, Actor* on_actor, TESObjectARMA* on_item, BSFixedString physics_file_path, bool persist, bool debug);

		bool SwapPhysicsFile(StaticFunctionTag* base, Actor* on_actor, BSFixedString old_physics_file_path, BSFixedString new_physics_file_path, bool persist, bool verbose_log);

		BSFixedString QueryCurrentPhysicsFile(StaticFunctionTag* base, Actor* on_actor, TESObjectARMA* on_item, bool verbose_log);
	}

}