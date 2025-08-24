#include <Windows.h>
#include <iostream>
#include <chrono>
#include <fstream>

#include "Generators/CppGenerator.h"
#include "Generators/MappingGenerator.h"
#include "Generators/IDAMappingGenerator.h"
#include "Generators/DumpspaceGenerator.h"

#include "Generators/Generator.h"
#include <thread>

#include "QuickDllProxy/DllProxy.h"

enum class EFortToastType : uint8
{
        Default                        = 0,
        Subdued                        = 1,
        Impactful                      = 2,
        EFortToastType_MAX             = 3,
};

static int Dump(HMODULE Module) {
	AllocConsole();
	FILE* Dummy;
	freopen_s(&Dummy, "CONOUT$", "w", stderr);
	freopen_s(&Dummy, "CONIN$", "r", stdin);

	auto t_1 = std::chrono::high_resolution_clock::now();

	std::cerr << "Started Generation [Dumper-7]!\n";
	Generator::InitEngineCore();
	Generator::InitInternal();

	if (Settings::Generator::GameName.empty() && Settings::Generator::GameVersion.empty())
	{
		// Only Possible in Main()
		FString Name;
		FString Version;
		UEClass Kismet = ObjectArray::FindClassFast("KismetSystemLibrary");
		UEFunction GetGameName = Kismet.GetFunction("KismetSystemLibrary", "GetGameName");
		UEFunction GetEngineVersion = Kismet.GetFunction("KismetSystemLibrary", "GetEngineVersion");

		Kismet.ProcessEvent(GetGameName, &Name);
		Kismet.ProcessEvent(GetEngineVersion, &Version);

		Settings::Generator::GameName = Name.ToString();
		Settings::Generator::GameVersion = Version.ToString();
		Settings::Internal::bUseLargeWorldCoordinates = Version.ToString().find("UE4") != std::string::npos ? false : true;
	}

	std::cerr << "GameName: " << Settings::Generator::GameName << "\n";
	std::cerr << "GameVersion: " << Settings::Generator::GameVersion << "\n\n";
	Generator::Generate<MappingGenerator>();
	Generator::Generate<CppGenerator>();
	Generator::Generate<IDAMappingGenerator>();
	Generator::Generate<DumpspaceGenerator>();

	auto t_C = std::chrono::high_resolution_clock::now();

	auto ms_int_ = std::chrono::duration_cast<std::chrono::milliseconds>(t_C - t_1);
	std::chrono::duration<double, std::milli> ms_double_ = t_C - t_1;

	std::cerr << "\n\nGenerating SDK took (" << ms_double_.count() << "ms)\n\n\n";

	Sleep(100);
	fclose(stderr);
	if (Dummy) fclose(Dummy);
	FreeConsole();

	FreeLibraryAndExitThread(Module, 0);
	return 0;
}

std::atomic<bool> keyPressed = false;
void listener()
{
	while (!keyPressed)
	{
		keyPressed = GetAsyncKeyState(Settings::Config::DumpKey) & 0x8000;
	
	}
	Sleep(50);
}

DWORD MainThread(HMODULE Module) {
	static auto t_0 = std::chrono::high_resolution_clock::now();
	Settings::Config::Load();
	char dumper[256];
	// Kind of pointless but it allows the dll to be renamed and its easier to get that here while we already have the handle
	GetModuleFileNameA(Module, dumper, sizeof(dumper));
	Settings::Config::ModulePath = dumper;
	if (Settings::Config::DumpKey == 0) {
		Sleep(Settings::Config::SleepTimeout);
		return Dump(Module);
	}

	std::thread listenerThread(listener);
	listenerThread.detach();
	auto t_x = std::chrono::high_resolution_clock::now();
	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_x - t_0).count();
	while (!keyPressed) {
		if (Settings::Config::SleepTimeout > 0) {
			t_x = std::chrono::high_resolution_clock::now();
			diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_x - t_0).count();
			if (diff >= Settings::Config::SleepTimeout) {
				std::cerr << "Timeout reached, dumping SDK...\n";
				return Dump(Module);
			}
		}
	Sleep(100);
	}
	return Dump(Module);

}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
	switch (reason)
	{

	case DLL_PROCESS_ATTACH:
		 DisableThreadLibraryCalls(hModule);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
		break;
	}

	return TRUE;
}

LRESULT CALLBACK NextHook(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam) {
	CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, GetModuleHandleA("Dumper-7.dll"), 0, nullptr);
	return CallNextHookEx(0, nCode, wParam, lParam);
}
