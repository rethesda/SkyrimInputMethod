#pragma once

#include <wrl/client.h>

#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <vector>

#include "directxtk/CommonStates.h"
#include "directxtk/SpriteFont.h"

#include "ICriticalSection.h"

typedef std::vector<std::wstring> CandidateList;
#define MAX_CANDLIST 8

#define SCI_ES_DISABLE 0

#define SCI_DKS_DISABLE

class InGameIME final : public Singleton<InGameIME>
{
public:
	InGameIME();

	bool Initialize(IDXGISwapChain* swapchain, ID3D11Device* a_device, ID3D11DeviceContext* a_device_context);

	void OnLoadConfig();

	// D3D Events
	void NextGroup();
	void OnResetDevice();
	void OnLostDevice();
	HRESULT OnRender();
	HRESULT OnResizeTarget();

	volatile std::uintptr_t enableState = SCI_ES_DISABLE;
	volatile std::uintptr_t disableKeyState = false;
	volatile std::uintptr_t pageStartIndex = 0;
	volatile std::uintptr_t selectedIndex = -1;

	std::string stateInfo;
	/// <summary>
	/// 输入的内容, 需要调用DrawTextW()来绘制，所以是宽字符
	/// </summary>
	std::wstring inputContent;
	/// <summary>
	/// 候选字列表
	/// </summary>
	CandidateList candidateList;

	bool IsImeOpen();
	[[nodiscard]] inline bool Initialized() { return mInitialized; }

	void ProcessImeComposition(HWND hWnd, WPARAM wParam, LPARAM lParam);
	void ProcessImeNotify(HWND hWnd, WPARAM wParam, LPARAM lParam);

	HWND hwnd;

private:
	bool mInitialized = false;

	IDXGISwapChain* pSwapChain = nullptr;
	ID3D11Device* pDevice = nullptr;
	ID3D11DeviceContext* pDeviceContext = nullptr;

	IDXGIFactory* pFactory = nullptr;

	IDWriteTextFormat* pHeaderFormat;
	IDWriteTextFormat* pInputContentFormat;
	IDWriteTextFormat* pCandicateItemFormat;

	D2D1_ROUNDED_RECT widgetRect = D2D1::RoundedRect(D2D1::RectF(0.0f, 0.0f, 200.0f, 400.0f), 10.0f, 10.0f);
	D2D_RECT_F headerRect = D2D1::RectF(0.0f, 0.0f, 200.0f, 200.0f);
	D2D_RECT_F inputContentRect = D2D1::RectF(0.0f, 200.0f, 200.0f, 400.0f);

	ID2D1SolidColorBrush* m_pWhiteColorBrush;
	ID2D1SolidColorBrush* m_pHeaderColorBrush;
	ID2D1LinearGradientBrush* m_pBackgroundBrush;

	ID2D1Factory* m_pD2DFactory;
	IDWriteFactory* m_pDWFactory;
	ID2D1RenderTarget* m_pBackBufferRT = nullptr;
	D2D1_POINT_2F origin;

	ID3D11BlendState* m_pBlendState;

	HRESULT CreateD2DResources();
};

extern ICriticalSection ime_critical_section;