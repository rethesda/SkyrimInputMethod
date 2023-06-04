#include "Hooks/SKSEManager.h"

#include "Hooks/InputManager.h"
#include "RE/Offset.h"

namespace Hooks
{
	void SKSEManager::Install()
	{
		INFO("Installing SKSEManager.");
		auto hook = REL::Relocation<std::uintptr_t>(RE::Offset::BSScaleformManager::LoadMovie, 0x1DD);

		if (!REL::make_pattern<"FF 15">().match(hook.address())) {
			ERROR("Failed to install SKSEManager::ScaleformPatch");
		}
		DEBUG("Hooking BSScaleformManager::LoadMovie")
		auto& trampoline = SKSE::GetTrampoline();
		SKSE::AllocTrampoline(14);
		auto ptr = trampoline.write_call<6>(hook.address(), &RegisterScaleformFunctions);
		_SetViewScaleMode = *reinterpret_cast<std::uintptr_t*>(ptr);
		INFO("Installed.");
	}

	void SKSEManager::RegisterScaleformFunctions(RE::GFxMovieView* a_view, RE::GFxMovieView::ScaleModeType a_scaleMode)
	{
		_SetViewScaleMode(a_view, a_scaleMode);

		RE::GFxValue skse;
		a_view->GetVariable(&skse, "_global.skse");

		if (!skse.IsObject()) {
			ERROR("Failed to get _global.skse");
		}
		RE::GFxValue fn_AllowTextInput;
		static auto AllowTextInput = new SKSEScaleform_AllowTextInput;
		a_view->CreateFunction(&fn_AllowTextInput, AllowTextInput);
		skse.SetMember("AllowTextInput", fn_AllowTextInput);
	}

	void SKSEScaleform_AllowTextInput::Call(Params& a_params)
	{
		using namespace RE;

		ControlMap* control_map = ControlMap::GetSingleton();
		Hooks::InputManager* input_manager = InputManager::GetSingleton();

		assert(a_params.argCount >= 1);
		bool enable = a_params.args[0].GetBool();

		if (control_map) {
			input_manager->ProcessAllowTextInput(enable);
			control_map->AllowTextInput(enable);
		} else {
			INFO("ControlMap not initialized");
		}
	}
}