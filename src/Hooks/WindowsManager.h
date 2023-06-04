#pragma once

namespace Hooks
{
	class WindowsManager final : public Singleton<WindowsManager>
	{
	public:
		void Install();

	private:
		/*===========================
		==========  Hooks  ==========
		=============================*/

		struct WindowProc_Hook
		{
			static HRESULT __fastcall hooked(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

			static inline decltype(&hooked) oldFunc;
		};
	};
}