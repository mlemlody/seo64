#include "game/game.hpp"

#include "app/features/notifs/notifs.hpp"

constexpr int errc_noerr{ 0 };
constexpr int errc_sigs{ 1 };
constexpr int errc_ifaces{ 2 };
constexpr int errc_cvars{ 3 };
constexpr int errc_hasload{ 4 };
constexpr int errc_hasunload{ 5 };
constexpr int errc_hooks{ 6 };

void dump_error_string(const std::string& errors, const bool copyToClipboard)
{
	MessageBoxA(nullptr, errors.c_str(), "seo64", MB_OK | MB_ICONERROR);

	if ( copyToClipboard )
	{
		const char* output = errors.c_str();
		const size_t len = errors.size() + 1; // + null terminator

		// Allocate movable memory
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
		if ( !hMem )
			return;

		// Lock the memory and copy the error string into it
		LPVOID pMem = GlobalLock(hMem);
		if ( !pMem )
		{
			GlobalFree(hMem);
			return;
		}
		memcpy(pMem, output, len);
		GlobalUnlock(hMem);

		// Open the clipboard and set the data
		if ( OpenClipboard(nullptr) )
		{
			if ( EmptyClipboard() )
			{
				if ( SetClipboardData(CF_TEXT, hMem) == nullptr )
					GlobalFree(hMem);
			}
			else
				GlobalFree(hMem);

			CloseClipboard();
		}
		else
			GlobalFree(hMem);
	}
}

int load()
{
	std::string errorSigs{}, errorIfaces{};
	bool errSigs = false, errIfaces = false;

	for ( Sig* const s : getInsts<Sig>() )
	{
		if ( !s || !s->init() ) {
			if ( !errSigs )
				errSigs = true;

			errorSigs += std::format("{}\n", s->name());
		}
	}
	if ( errSigs )
	{
		dump_error_string(errorSigs, true);
		return errc_sigs;
	}

	for ( Iface* const i : getInsts<Iface>() )
	{
		if ( !i || !i->init() ) {
			if ( !errIfaces )
				errIfaces = true;

			errorIfaces += std::format("{}\n", i->name());
		}
	}
	if ( errIfaces )
	{
		dump_error_string(errorIfaces, true);
		return errc_ifaces;
	}

	for (CvarInit *const c : getInsts<CvarInit>())
	{
		if (!c || !c->init()) {
			return errc_cvars;
		}
	}

	for (Font *const f : getInsts<Font>())
	{
		if (!f) {
			continue;
		}

		f->init();
	}

	cfg::load(utils::getFilePath() + "cfg\\default.json");

	if (std::filesystem::exists(utils::getFilePath() + "cfg\\default.json")) {
		notifs->message("loaded 'default'");
	}

	for (HasLoad *const l : getInsts<HasLoad>())
	{
		if (!l || !l->onLoad()) {
			return errc_hasload;
		}
	}

	if (MH_Initialize() != MH_OK) {
		return errc_hooks;
	}

	for (Hook *const h : getInsts<Hook>())
	{
		if (!h || !h->init()) {
			return errc_hooks;
		}
	}

	if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
		return errc_hooks;
	}

	std::string str = "Greetings from a different thread.";

	//ThreadExecute(&thread_test, str);

	return errc_noerr;
}

int unload()
{
	if (MH_Uninitialize() != MH_OK) {
		return errc_hooks;
	}

	for (HasUnload *const u : getInsts<HasUnload>())
	{
		if (!u || !u->onUnload()) {
			return errc_hasunload;
		}
	}

	return errc_noerr;
}

DWORD WINAPI mainThread(LPVOID param)
{
	if (const int errc{ load() }) {
		MessageBoxA(0, std::format("failed to load ({})", errc).c_str(), "seo64", 0);
		FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(param), EXIT_FAILURE);
	}

	while (!GetAsyncKeyState(VK_F11)) {
		Sleep(500);
	}

	if (const int errc{ unload() }) {
		MessageBoxA(0, std::format("failed to unload ({})", errc).c_str(), "seo64", 0);
		FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(param), EXIT_FAILURE);
	}

	Sleep(500);

	FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(param), EXIT_SUCCESS);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
	if (reason != DLL_PROCESS_ATTACH) {
		return FALSE;
	}

	const HANDLE main_thread{ CreateThread(0, 0, mainThread, inst, 0, 0) };

	if (!main_thread) {
		return FALSE;
	}

	CloseHandle(main_thread);

	return TRUE;
}