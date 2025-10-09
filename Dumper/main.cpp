#include <Windows.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <string>
#include <Settings.h>
#include <thread>
#include <Windows.h>
#include "Generators/CppGenerator.h"
#include "Generators/MappingGenerator.h"
#include "Generators/IDAMappingGenerator.h"
#include "Generators/DumpspaceGenerator.h"

#include "Generators/Generator.h"


 // Using an atomic bool allows us to reliably wait on another thread without race conditions
 std::atomic<bool> dumpStarted = false;
 std::atomic<bool> foundUEVR = false;
 // Use the actual value for VK scancodes, e.g. 0x77 = F8
 void ListenerThread(int key)
 {
 	while (!dumpStarted)
 	{
 		dumpStarted = GetAsyncKeyState(key) & 0x8000;
 	}
 }


enum class EFortToastType : uint8
{
	Default = 0,
	Subdued = 1,
	Impactful = 2,
	EFortToastType_MAX = 3,
};

namespace cfg = Settings::Config;
extern "C" {
	#include "I:/code/UEVR/include/uevr/API.h"
}

static void UEVR()
{
	std::cerr << "Checking for UEVR" << std::endl;
	const auto unreal_vr_backend = GetModuleHandleA("UEVRBackend.dll");
	if (unreal_vr_backend == nullptr)
		return;
	foundUEVR = true;
	std::cerr << "Found uevr" << std::endl;
	const UEVR_PluginInitializeParam*  m_plugin_initialize_param = (UEVR_PluginInitializeParam*)GetProcAddress(unreal_vr_backend, "g_plugin_initialize_param");
	const auto m_sdk{ m_plugin_initialize_param->sdk };
	if (m_sdk == nullptr)
		return;
	const auto m_functions{ m_plugin_initialize_param->functions };
	if (m_functions == nullptr)
		return;

	const auto size = m_plugin_initialize_param->functions->get_persistent_dir(nullptr, 0);
	std::wstring result(size, L'\0');
	m_plugin_initialize_param->functions->get_persistent_dir(result.data(), size + 1);
	std::filesystem::path output{result};
	output /= "Dumper7SDK";
	std::cerr << "SDKGeneration Path" << output << std::endl;
	cfg::DumpKey = 0x77;
	cfg::SleepTimeout = 0;
	Settings::Generator::SDKGenerationPath = output.string();
	return;
}

DWORD MainThread(HMODULE Module)
{
	AllocConsole();
	FILE* Dummy;
	freopen_s(&Dummy, "CONOUT$", "w", stderr);
	freopen_s(&Dummy, "CONIN$", "r", stdin);

	using namespace std::chrono;
	static auto t_0 = high_resolution_clock::now();

	
	std::thread uevr_thread(&UEVR);
	uevr_thread.join();
	if (!foundUEVR) cfg::Load();
	

	 if (cfg::DumpKey != 0)
	 {
	 	std::thread listenerThread(&ListenerThread, cfg::DumpKey);
	 	// detach the thread and loop so we don't unload or initiate the dump unprompted
	 	listenerThread.detach();
	 	while (!dumpStarted)
	 	{	
			if (cfg::SleepTimeout != 0)
	 		{
				if (duration_cast<milliseconds>(high_resolution_clock::now() - t_0).count() > cfg::SleepTimeout)
				{
	 				dumpStarted = true;
					break;
				}	 		
			}
	 		// sleep a bit to ensure we aren't checking every ms
	 		Sleep(50);
	 	}
	 }
	 else
	 {
	 	// no need to check the value since sleeping for 0 ms does nothing
	 	Sleep(cfg::SleepTimeout);
	 }


	std::cerr << "Started Generation [Dumper-7]!\n";
	auto t_1 = high_resolution_clock::now();
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
	}

	std::cerr << "GameName: " << Settings::Generator::GameName << "\n";
	std::cerr << "GameVersion: " << Settings::Generator::GameVersion << "\n\n";

	Generator::Generate<CppGenerator>();
	Generator::Generate<MappingGenerator>();
	Generator::Generate<IDAMappingGenerator>();
	Generator::Generate<DumpspaceGenerator>();

	auto t_C = high_resolution_clock::now();

	auto ms_int_ = duration_cast<milliseconds>(t_C - t_1);
	duration<double, std::milli> ms_double_ = t_C - t_1;

	std::cerr << "\n\nGenerating SDK took (" << ms_double_.count() << "ms)\n\n\n";

	while (true)
	{
		// If a dump key was set we can reuse that key, otherwise default to F6
		if (GetAsyncKeyState(cfg::DumpKey == 0 ? VK_F6 : cfg::DumpKey) & 1)
		{
			fclose(stderr);
			if (Dummy) fclose(Dummy);
			FreeConsole();

			FreeLibraryAndExitThread(Module, 0);
		}

		Sleep(100);
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
		break;
	}

	return TRUE;
}
