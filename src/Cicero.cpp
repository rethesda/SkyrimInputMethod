#include "Cirero.h"

#include "Helpers/DebugHelper.h"
#include "InGameIme.h"
#include "Utils.h"

bool GetLayoutName(const WCHAR* kl, WCHAR* nm)
{
	long lRet;
	HKEY hKey;
	static WCHAR tchData[64];
	DWORD dwSize;
	WCHAR keypath[200];
	wsprintfW(keypath, L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\%s", kl);
	lRet = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keypath, 0, KEY_QUERY_VALUE, &hKey);
	if (lRet == ERROR_SUCCESS) {
		dwSize = sizeof(tchData);
		lRet = RegQueryValueExW(hKey, L"Layout Text", NULL, NULL, (LPBYTE)tchData, &dwSize);
	}
	RegCloseKey(hKey);
	if (lRet == ERROR_SUCCESS && wcslen(nm) < 64) {
		wcscpy(nm, tchData);
		return true;
	}
	return false;
}

Cicero::Cicero() :
	ciceroState(CICERO_DISABLED),
	m_pProfileMgr(nullptr),
	m_pProfiles(nullptr),
	m_pThreadMgr(nullptr),
	m_pThreadMgrEx(nullptr),
	m_pBaseContext(nullptr),
	m_refCount(1)
{
	m_uiElementSinkCookie = m_inputProcessorProfileActivationSinkCookie = m_threadMgrEventSinkCookie = m_textEditSinkCookie = TF_INVALID_COOKIE;
}

Cicero::~Cicero()
{
}

STDAPI_(ULONG)
Cicero::AddRef()
{
	return ++m_refCount;
}

STDAPI_(ULONG)
Cicero::Release()
{
	ULONG result = --m_refCount;
	if (result == 0)
		delete this;
	return result;
}

STDAPI Cicero::QueryInterface(REFIID riid, void** ppvObj)
{
	if (ppvObj == nullptr)
		return E_INVALIDARG;
	*ppvObj = nullptr;
	if (IsEqualIID(riid, IID_IUnknown))
		*ppvObj = reinterpret_cast<IUnknown*>(this);
	else if (IsEqualIID(riid, IID_ITfUIElementSink))
		*ppvObj = (ITfUIElementSink*)this;
	else if (IsEqualIID(riid, IID_ITfInputProcessorProfileActivationSink))
		*ppvObj = (ITfInputProcessorProfileActivationSink*)this;
	else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
		*ppvObj = (ITfThreadMgrEventSink*)this;
	else if (IsEqualIID(riid, IID_ITfTextEditSink))
		*ppvObj = (ITfTextEditSink*)this;
	else
		return E_NOINTERFACE;
	AddRef();
	return S_OK;
}

HRESULT Cicero::SetupSinks()
{
	HRESULT hr = S_OK;
	ITfSource* pSource;

	hr = CoInitialize(NULL);
	if (SUCCEEDED(hr)) {
		hr = CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pThreadMgrEx));
	}
	if (SUCCEEDED(hr)) {
		hr = m_pThreadMgrEx->ActivateEx(&m_clientID, TF_TMAE_UIELEMENTENABLEDONLY);
	}
	if (SUCCEEDED(hr)) {
		hr = m_pThreadMgrEx->QueryInterface(IID_ITfSource, (LPVOID*)&pSource);
	}
	if (SUCCEEDED(hr)) {
		pSource->AdviseSink(__uuidof(ITfUIElementSink), (ITfUIElementSink*)this, &m_uiElementSinkCookie);
		pSource->AdviseSink(__uuidof(ITfInputProcessorProfileActivationSink), (ITfInputProcessorProfileActivationSink*)this, &m_inputProcessorProfileActivationSinkCookie);
		pSource->AdviseSink(__uuidof(ITfThreadMgrEventSink), (ITfThreadMgrEventSink*)this, &m_threadMgrEventSinkCookie);

		pSource->Release();

		hr = CoCreateInstance(
			CLSID_TF_InputProcessorProfiles,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_ITfInputProcessorProfiles,
			(LPVOID*)&m_pProfiles);
	}
	if (SUCCEEDED(hr)) {
		hr = m_pProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr, (LPVOID*)&m_pProfileMgr);
	}
	UpdateCurrentInputMethodName();
	if (SUCCEEDED(hr)) {
		INFO("[Cicero] set-up Successful");
	} else {
		ERROR("[Cicero] Set-up Failed");
	}
	return S_OK;
}

void Cicero::ReleaseSinks()
{
	HRESULT hr = S_OK;
	ITfSource* pSource = nullptr;
	if (m_textEditSinkCookie != TF_INVALID_COOKIE) {
		if (m_pBaseContext && SUCCEEDED(m_pBaseContext->QueryInterface(&pSource))) {
			hr = pSource->UnadviseSink(m_textEditSinkCookie);
			SafeRelease(&pSource);
			SafeRelease(&m_pBaseContext);
			m_textEditSinkCookie = TF_INVALID_COOKIE;
		}
	}
	if (m_pThreadMgrEx && SUCCEEDED(m_pThreadMgrEx->QueryInterface(IID_PPV_ARGS(&pSource)))) {
		pSource->UnadviseSink(m_uiElementSinkCookie);
		pSource->UnadviseSink(m_inputProcessorProfileActivationSinkCookie);
		pSource->UnadviseSink(m_threadMgrEventSinkCookie);
		m_pThreadMgrEx->Deactivate();
		SafeRelease(&pSource);
		SafeRelease(&m_pThreadMgr);
		SafeRelease(&m_pProfileMgr);
		SafeRelease(&m_pProfiles);
		SafeRelease(&m_pThreadMgrEx);
	}
	CoUninitialize();
	INFO("[Cicero] Released Sinks");
}

// ITfUIElementSink::BeginUIElement
STDAPI Cicero::BeginUIElement(DWORD dwUIElementId, BOOL* pbShow)
{
	int token = rand();
	DH_DEBUG("[Cicero#BeginUIElement#{}] *CallBack*", token);
	HRESULT hr = S_OK;

	InGameIME* pInGameIme = InGameIME::GetSingleton();
	InterlockedExchange(&pInGameIme->enableState, 1);

	ITfUIElement* pElement = GetUIElement(dwUIElementId);

	if (!pElement) {
		return E_INVALIDARG;
	}
	*pbShow = FALSE;
	ITfCandidateListUIElement* lpCandidate = nullptr;
	hr = pElement->QueryInterface(IID_PPV_ARGS(&lpCandidate));
	if (SUCCEEDED(hr)) {
		this->UpdateCandidateList(lpCandidate);
	}
	pElement->Release();
	DH_DEBUG("[Cicero#BeginUIElement#{}] *CallBack* end with result {}", token, hr);
	return S_OK;
}

// ITfUIElementSink::UpdateUIElement
STDAPI Cicero::UpdateUIElement(DWORD dwUIElementId)
{
	DH_DEBUG("[Cicero UpdateUIElement] *CallBack*");
	HRESULT hr = S_OK;
	ITfUIElement* pElement = GetUIElement(dwUIElementId);
	if (!pElement) {
		return E_INVALIDARG;
	}
	ITfCandidateListUIElement* lpCandidate = nullptr;
	hr = pElement->QueryInterface(IID_PPV_ARGS(&lpCandidate));
	if (SUCCEEDED(hr)) {
		this->UpdateCandidateList(lpCandidate);
		pElement->Release();
	}
	return S_OK;
}

// ITfUIElementSink::EndUIElement
STDAPI Cicero::EndUIElement(DWORD dwUIElementId)
{
	DH_DEBUG("[Cicero EndUIElement] *CallBack*");
	HRESULT hr = S_OK;
	ITfUIElement* pElement = GetUIElement(dwUIElementId);
	if (!pElement) {
		return E_INVALIDARG;
	}
	ITfCandidateListUIElement* lpCandidate = nullptr;
	hr = pElement->QueryInterface(IID_PPV_ARGS(&lpCandidate));
	if (SUCCEEDED(hr)) {
		this->UpdateCandidateList(lpCandidate);
		pElement->Release();
	}
	return S_OK;
}

// ITfTextEditSink
STDAPI Cicero::OnEndEdit(ITfContext* cxt, TfEditCookie ecReadOnly, ITfEditRecord* pEditRecord)
{
	DH_DEBUG("[Cicero OnEndEdit] *CallBack* ");
	ITfContextComposition* pContextComposition;
	if (FAILED(cxt->QueryInterface(&pContextComposition)))
		return S_OK;
	IEnumITfCompositionView* pEnumCompositionView;
	if (FAILED(pContextComposition->EnumCompositions(&pEnumCompositionView))) {
		pContextComposition->Release();
		return S_OK;
	}
	ITfCompositionView* pCompositionView;
	ULONG remainCount = 0;
	while (SUCCEEDED(pEnumCompositionView->Next(1, &pCompositionView, &remainCount)) && remainCount > 0) {
		ITfRange* pRange;
		WCHAR endEditBuffer[MAX_PATH];
		ULONG bufferSize = 0;
		if (SUCCEEDED(pCompositionView->GetRange(&pRange))) {
			pRange->GetText(ecReadOnly, TF_TF_MOVESTART, endEditBuffer, MAX_PATH, &bufferSize);
			endEditBuffer[bufferSize] = '\0';

			std::wstring result(endEditBuffer);

			InGameIME* inGameIme = InGameIME::GetSingleton();
			ime_critical_section.Enter();
			DH_DEBUGW(L"Input Content", result);
			inGameIme->inputContent = result;
			ime_critical_section.Leave();

			pRange->Release();
		}
		pCompositionView->Release();
	}
	pEnumCompositionView->Release();
	pContextComposition->Release();
	return S_OK;
}

// ITfInputProcessorProfileActivationSink
STDAPI Cicero::OnActivated(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID catid, REFGUID guidProfile, HKL hkl, DWORD dwFlags)
{
	DH_DEBUG("[Cicero#OnActivated] *CallBack*");
	if (!(dwFlags & TF_IPSINK_FLAG_ACTIVE))
		return S_OK;
	this->UpdateCurrentInputMethodName();
	if (dwProfileType & TF_PROFILETYPE_INPUTPROCESSOR) {
		DEBUG("[Cicero#OnActivated] TIP输入法 {}", (unsigned int)hkl);
		ciceroState = CICERO_ENABLED;
	} else if (dwProfileType & TF_PROFILETYPE_KEYBOARDLAYOUT) {
		DEBUG("[Cicero#OnActivated] HKL/IME {}", (unsigned int)hkl);
		ciceroState = CICERO_DISABLED;
	} else {
		DEBUG("[Cicero#OnActivated] Unknown Profile Type {}", (unsigned int)hkl);
		ciceroState = CICERO_DISABLED;
	}
	DEBUG("[Cicero#OnActivated] {} cicero input method", ciceroState ? "Activated" : "Deactivated");
	return S_OK;
}

// ITfThreadMgrEventSink::OnInitDocumentMgr
STDAPI Cicero::OnInitDocumentMgr(ITfDocumentMgr* pdim)
{
	return S_OK;
}

// ITfThreadMgrEventSink::OnUninitDocumentMgr
STDAPI Cicero::OnUninitDocumentMgr(ITfDocumentMgr* pdim)
{
	return S_OK;
}

// ITfThreadMgrEventSink::OnSetFocus
STDAPI Cicero::OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr* pdimPrevFocus)
{
	int token = rand();
	DH_DEBUG("[Cicero#OnSetFocus#{}] *CallBack*", token);
	HRESULT hr = S_OK;
	if (!pdimFocus)
		return S_OK;
	ITfSource* pSource = nullptr;
	if (m_textEditSinkCookie != TF_INVALID_COOKIE) {
		if (m_pBaseContext && SUCCEEDED(m_pBaseContext->QueryInterface(&pSource))) {
			DEBUG("[Cicero#OnSetFocus#{}] UnadviceSink", token);
			hr = pSource->UnadviseSink(m_textEditSinkCookie);
			SafeRelease(&pSource);
			if (FAILED(hr)) {
				DEBUG("[Cicero#OnSetFocus#{}] UnadviceSink Failed", token);
				return S_OK;
			}
			SafeRelease(&m_pBaseContext);
			m_textEditSinkCookie = TF_INVALID_COOKIE;
		}
	}
	hr = pdimFocus->GetBase(&m_pBaseContext);
	if (SUCCEEDED(hr)) {
		hr = m_pBaseContext->QueryInterface(&pSource);
	}
	if (SUCCEEDED(hr)) {
		DH_DEBUG("[Cicero#OnSetFocus#{}] AdviseSink", token);
		hr = pSource->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this), &m_textEditSinkCookie);
	}
	SafeRelease(&pSource);
	DH_DEBUG("[Cicero#OnSetFocus#{}] HRESULT: {}", token, hr);
	return S_OK;
}

// ITfThreadMgrEventSink::OnPushContext
STDAPI Cicero::OnPushContext(ITfContext* pic)
{
	return S_OK;
}

// ITfThreadMgrEventSink::OnPopContext
STDAPI Cicero::OnPopContext(ITfContext* pic)
{
	return S_OK;
}

ITfUIElement* Cicero::GetUIElement(DWORD dwUIElementId)
{
	ITfUIElementMgr* pElementMgr = nullptr;
	ITfUIElement* pElement = nullptr;
	if (SUCCEEDED(m_pThreadMgrEx->QueryInterface(__uuidof(ITfUIElementMgr), (void**)&pElementMgr))) {
		pElementMgr->GetUIElement(dwUIElementId, &pElement);
		pElementMgr->Release();
	}
	return pElement;
}

void Cicero::UpdateCandidateList(ITfCandidateListUIElement* lpCandidate)
{
	InGameIME* inGameIme = InGameIME::GetSingleton();

	if (InterlockedCompareExchange(&inGameIme->enableState, inGameIme->enableState, 2)) {
		UINT uIndex = 0, uCount = 0, uCurrentPage = 0, uPageCount = 0;
		DWORD dwPageStart = 0, dwPageSize = 0, dwPageSelection = 0;

		BSTR result = nullptr;

		lpCandidate->GetSelection(&uIndex);
		lpCandidate->GetCount(&uCount);
		lpCandidate->GetCurrentPage(&uCurrentPage);

		dwPageSelection = static_cast<DWORD>(uIndex);
		lpCandidate->GetPageIndex(nullptr, 0, &uPageCount);
		if (uPageCount > 0) {
			std::unique_ptr<UINT[], void(__cdecl*)(void*)> indexList(
				reinterpret_cast<UINT*>(Utils::HeapAlloc(sizeof(UINT) * uPageCount)),
				Utils::HeapFree);
			lpCandidate->GetPageIndex(indexList.get(), uPageCount, &uPageCount);
			dwPageStart = indexList[uCurrentPage];
			dwPageSize = (uCurrentPage < uPageCount - 1) ? std::min(uCount, indexList[uCurrentPage + 1]) - dwPageStart : uCount - dwPageStart;
		}
		dwPageSize = std::min<DWORD>(dwPageSize, MAX_CANDLIST);
		if (dwPageSize)
			InterlockedExchange(&inGameIme->disableKeyState, 1);

		// Update Candidate Page Information to In Game Menu
		InterlockedExchange(&inGameIme->selectedIndex, dwPageSelection);
		InterlockedExchange(&inGameIme->pageStartIndex, dwPageStart);

		WCHAR candidateBuffer[MAX_PATH];
		WCHAR resultBuffer[MAX_PATH];

		ime_critical_section.Enter();
		inGameIme->candidateList.clear();
		for (int i = 0; i < dwPageSize; i++) {
			if (SUCCEEDED(lpCandidate->GetString(i + dwPageStart, &result))) {
				if (result != nullptr) {
					wsprintf(candidateBuffer, L"%d. %s", i + 1, result);
					DH_DEBUGW(L"候选字 {}", candidateBuffer);
					std::wstring temp(candidateBuffer);

					inGameIme->candidateList.push_back(temp);
					SysFreeString(result);
				}
			}
		}
		ime_critical_section.Leave();
	}
}

void Cicero::UpdateCurrentInputMethodName()
{
	static WCHAR lastTipName[64];

	ZeroMemory(lastTipName, sizeof(lastTipName));
	TF_INPUTPROCESSORPROFILE tip;
	m_pProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &tip);
	if (tip.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR) {
		BSTR bstrImeName = nullptr;
		m_pProfiles->GetLanguageProfileDescription(tip.clsid, tip.langid, tip.guidProfile, &bstrImeName);
		if (bstrImeName != nullptr && wcslen(bstrImeName) < 64)
			wcscpy(lastTipName, bstrImeName);
		if (bstrImeName != nullptr)
			SysFreeString(bstrImeName);
	} else if (tip.dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT) {
		static WCHAR klnm[KL_NAMELENGTH];
		if (GetKeyboardLayoutNameW(klnm)) {
			GetLayoutName(klnm, lastTipName);
		}
	}

	int len = WideCharToMultiByte(CP_ACP, 0, lastTipName, wcslen(lastTipName), NULL, 0, NULL, NULL);
	std::unique_ptr<char[], void(__cdecl*)(void*)> resultBuffer(
		reinterpret_cast<char*>(Utils::HeapAlloc(len + 1)),
		Utils::HeapFree);

	WideCharToMultiByte(CP_ACP, 0, lastTipName, wcslen(lastTipName), resultBuffer.get(), len, NULL, NULL);
	resultBuffer[len] = '\0';

	std::string name = resultBuffer.get();

	std::uint32_t pos = name.find("-", NULL);
	std::string temp("状态: ");
	if (pos != std::string::npos) {
		pos += 2;
		temp.append(name, pos, name.size());
	} else
		temp.append(name);

	InGameIME* pInGameIME = InGameIME::GetSingleton();

	ime_critical_section.Enter();
	pInGameIME->stateInfo = temp;
	ime_critical_section.Leave();

	DH_DEBUG("当前输入法: {}", name);
}
