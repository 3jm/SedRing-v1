#include "hooks.h"

#include <stdexcept>
#include <intrin.h>


#include "../../ext/imgui/imgui.h"
#include "../../ext/imgui/imgui_impl_win32.h"
#include "../../ext/imgui/imgui_impl_dx9.h"

// include minhook for epic hookage
#include "../../ext/minhook/minhook.h"

#include <intrin.h>

void hooks::Setup() noexcept
{
	MH_Initialize();

	if (MH_Initialize())
		throw std::runtime_error("Unable to initialize minhook");

	if (MH_CreateHook(
		VirtualFunction(gui::device, 42),
		&EndScene,
		reinterpret_cast<void**>(&EndSceneOriginal)
	)) throw std::runtime_error("Unbale to hook EndScene(");

	if (MH_CreateHook(
		VirtualFunction(gui::device, 16),
		&Reset,
		reinterpret_cast<void**>(&ResetOriginal)
	)) throw std::runtime_error("Unbale to hook Reset(");

	if (MH_EnableHook(MH_ALL_HOOKS))
		throw std::runtime_error("Unable to enable hooks");

	// AllocKeyValuesMemory hook
	MH_CreateHook(
		memory::Get(interfaces::keyValuesSystem, 1),
		&AllocKeyValuesMemory,
		reinterpret_cast<void**>(&AllocKeyValuesMemoryOriginal)
	);

	// CreateMove hook
	MH_CreateHook(
		memory::Get(interfaces::clientMode, 24),
		&CreateMove,
		reinterpret_cast<void**>(&CreateMoveOriginal)
	);

	// PaintTraverse hook
	MH_CreateHook(
		memory::Get(interfaces::panel, 41),
		&PaintTraverse,
		reinterpret_cast<void**>(&PaintTraverseOriginal)
	);

	// DrawModel Hook
	MH_CreateHook(
		memory::Get(interfaces::studioRender, 29),
		&DrawModel,
		reinterpret_cast<void**>(&DrawModelOriginal)
	);

	MH_EnableHook(MH_ALL_HOOKS);
}

long __stdcall EndScene(IDirect3DDevice9* device) noexcept
{
	static const auto returnAddress = _ReturnAddress();
	
	const auto result = EndSceneOriginal(device, device);

	if (_ReturnAddress() == returnAddress)
		return result;
	
	if(!gui::setup)
		gui::SetupMenu(device);
	
	if(gui::open)
		gui::Render();

	return result;
}

HRESULT __stdcall Reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
	const auto result = ResetOriginal(device, device, params);
	ImGui_ImplDX9_CreateDeviceObjects();
	return result;
}

void hooks::Destroy() noexcept
{
	// restore hooks
	MH_DisableHook(MH_ALL_HOOKS);
	MH_RemoveHook(MH_ALL_HOOKS);

	// uninit minhook
	MH_Uninitialize();
}

void* __stdcall hooks::AllocKeyValuesMemory(const std::int32_t size) noexcept
{
	// if function is returning to speficied addresses, return nullptr to "bypass"
	if (const std::uint32_t address = reinterpret_cast<std::uint32_t>(_ReturnAddress());
		address == reinterpret_cast<std::uint32_t>(memory::allocKeyValuesEngine) ||
		address == reinterpret_cast<std::uint32_t>(memory::allocKeyValuesClient)) 
		return nullptr;

	// return original
	return AllocKeyValuesMemoryOriginal(interfaces::keyValuesSystem, size);
}

bool __stdcall hooks::CreateMove(float frameTime, CUserCmd* cmd) noexcept
{
	// make sure this function is being called from CInput::CreateMove
	if (!cmd->commandNumber)
		return CreateMoveOriginal(interfaces::clientMode, frameTime, cmd);

	// this would be done anyway by returning true
	if (CreateMoveOriginal(interfaces::clientMode, frameTime, cmd))
		interfaces::engine->SetViewAngles(cmd->viewAngles);

	// get our local player here
	globals::UpdateLocalPlayer();

	if (globals::localPlayer && globals::localPlayer->IsAlive())
	{
		// example bhop
		if (!(globals::localPlayer->GetFlags() & CEntity::FL_ONGROUND))
			cmd->buttons &= ~CUserCmd::IN_JUMP;
	}

	return false;
}

void __stdcall hooks::DrawModel(
	void* results,
	const CDrawModelInfo& info,
	CMatrix3x4* bones,
	float* flexWeights,
	float* flexDelayedWeights,
	const CVector& modelOrigin,
	const std::int32_t flags
	) noexcept
{
	if (globals::localPlayer && info.renderable)
	{
		CEntity* entity = info.renderable->GetIClientUnknown()->GetBaseEntity();
		if (entity && entity->IsPlayer() && entity->GetTeam() != globals::localPlayer->GetTeam())
		{
			static IMaterial* material = interfaces::materialSystem->FindMaterial("debug/debugambientcube");
			constexpr float hidden[3] = { 255.f, 0.f, 0.f };
			constexpr float visible[3] = { 55.f, 0.f, 255.f };

			interfaces::studioRender->SetAlphaModulation(1.f);

			material->SetMaterialVarFlag(IMaterial::IGNOREZ, true);
			interfaces::studioRender->SetColorModulation(hidden);
			interfaces::studioRender->ForcedMaterialOverride(material);
			DrawModelOriginal(interfaces::studioRender, results, info, bones, flexWeights, flexDelayedWeights, modelOrigin, flags);

			material->SetMaterialVarFlag(IMaterial::IGNOREZ, false);
			interfaces::studioRender->SetColorModulation(visible);
			interfaces::studioRender->ForcedMaterialOverride(material);
			DrawModelOriginal(interfaces::studioRender, results, info, bones, flexWeights, flexDelayedWeights, modelOrigin, flags);

			return interfaces::studioRender->ForcedMaterialOverride(nullptr);
		}
	}
	DrawModelOriginal(interfaces::studioRender, results, info, bones, flexWeights, flexDelayedWeights, modelOrigin, flags);
}

void __stdcall hooks::PaintTraverse(std::uintptr_t vguiPanel, bool forceRepaint, bool allowForce) noexcept
{
	if (vguiPanel == interfaces::engineVGui->GetPanel(PANEL_TOOLS))
	{
		if (globals::localPlayer && interfaces::engine->IsInGame())
		{
			for (int i = 1; i < interfaces::globals->maxClients; ++i)
			{
				CEntity* player = interfaces::entityList->GetEntityFromIndex(i);
				if (!player)
					continue;

				if (player->IsDormant() || !player->IsAlive())
					continue;

				if (player->GetTeam() == globals::localPlayer->GetTeam())
					continue;

				if (!globals::localPlayer->IsAlive())
					if (globals::localPlayer->GetObserverTarget() == player)
						continue;

				CMatrix3x4 bones[120];
				if (!player->SetupBones(bones, 120, 0x7FF00, interfaces::globals->currentTime))
					continue;

				CVector top;
				if (interfaces::debugOverlay->ScreenPosition(bones[8].Origin() + CVector{ 0.f, 0.f, 11.f }, top))
					continue;

				CVector bottom;
				if (interfaces::debugOverlay->ScreenPosition(player->GetAbsOrigin() - CVector{ 0.f, 0.f, 9.f }, bottom))
					continue;

				const float h = bottom.y - top.y;
				const float w = h * 0.3f;
				const auto left = static_cast<int>(top.x - w);
				const auto right = static_cast<int>(top.x + w);

				interfaces::surface->DrawSetColor(255, 255, 255, 255);

				interfaces::surface->DrawOutlinedRect(left, top.y, right, bottom.y);

				interfaces::surface->DrawSetColor(0, 0, 0, 255);

				interfaces::surface->DrawOutlinedRect(left - 1, top.y - 1, right + 1, bottom.y + 1);
				interfaces::surface->DrawOutlinedRect(left + 1, top.y + 1, right - 1, bottom.y - 1);

				interfaces::surface->DrawOutlinedRect(left - 6, top.y - 1, left - 3, bottom.y + 1);

				const float healthFrac = player->GetHealth() * 0.01f;

				interfaces::surface->DrawSetColor((1.f - healthFrac) * 255, 255 * healthFrac, 0, 255);

				interfaces::surface->DrawFilledRect(left - 5, bottom.y - (h * healthFrac), left - 4, bottom.y);
			}
		}
	}
	PaintTraverseOriginal(interfaces::panel, vguiPanel, forceRepaint, allowForce);
}