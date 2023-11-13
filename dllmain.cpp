#include <Windows.h>
#include "Injector/injector.hpp"
#include "IniReader/IniReader.h"

struct eTimeOfDayLighting
{
	float UpdateRate;
	int UpdateDirection;
	float CurrentTimeOfDay;

	static eTimeOfDayLighting* Instance()
	{
		return *(eTimeOfDayLighting**)0x00B77F34;
	}
};

struct Presset
{
	float Vertex1;
	float Vertex2;
	float Vertex3;
	float Sky;
	float Car;
	int Filter;
} Morning, Night, FrontEnd;

unsigned int FogColor = 0;
float ForceTime = -1.0f;
float LampFlareThreshold = 0.0f;
float LampFlareBackup = -1.0f;
bool MoonRotation = false;
bool LiveReload = false;
bool TimeSetting = false;
bool FilterTransition = false;

void InitPresset(CIniReader& iniReader, Presset& presset, const char* name)
{
	presset.Car = iniReader.ReadFloat(name, "Car", 1.0f);
	presset.Sky = iniReader.ReadFloat(name, "Sky", 1.0f);
	presset.Vertex1 = iniReader.ReadFloat(name, "Vertex1", 1.0f);
	presset.Vertex2 = iniReader.ReadFloat(name, "Vertex2", 1.0f);
	presset.Vertex3 = iniReader.ReadFloat(name, "Vertex3", 1.0f);
	presset.Filter = iniReader.ReadInteger(name, "Filter", 0);
}

void InitConfig()
{
	CIniReader iniReader("NFSCTimeOfNight.ini");

	InitPresset(iniReader, Morning, "MORNING");
	InitPresset(iniReader, Night, "NIGHT");
	InitPresset(iniReader, FrontEnd, "FRONTEND");

	ForceTime = iniReader.ReadFloat("GENERAL", "ForceTime", -1.0f);
	if (ForceTime > 1.0f)
	{
		ForceTime = 1.0f;
	}

	LampFlareThreshold = iniReader.ReadFloat("GENERAL", "LampFlareThreshold", 0.85f);

	MoonRotation = iniReader.ReadInteger("GENERAL", "MoonRotation", 0) == 1;

	LiveReload = iniReader.ReadInteger("GENERAL", "LiveReload", 0) == 1;

	TimeSetting = iniReader.ReadInteger("GENERAL", "TimeSetting", 0) == 1;

	FilterTransition = iniReader.ReadInteger("GENERAL", "FilterTransition", 0) == 1;

	FogColor = iniReader.ReadUInteger("GENERAL", "FogColor", 0);
}

float SkyBrightness = 1;
float WindowBrightness = 1;
float VertexBrightness = 1;
float FilterLerp = 0;
auto CarBrightness = (float*)0x009EA968;
auto WorldBrightness = (float*)0x00A6C204;
auto FogFallof = (float*)0x00B74228;
auto GameState = (int*)0x00A99BBC;
auto SkyRotation = (unsigned short*)0x00B77B78;
auto pFogColor = (int*)0x00B74234;
auto LampFlareSize = (float*)0x00A6BC14;
auto SkyCarReflection = (float*)0x00A63D1C;
auto SkyRoadReflection = (float*)0x00A63D18;
auto Filters = (int*)0x00B43068;
auto _UpdateTod = (void(__thiscall*)(eTimeOfDayLighting * _this))0x007F1080;

void __fastcall Update(eTimeOfDayLighting* tod)
{
	_UpdateTod(tod);

	if (LiveReload)
	{
		InitConfig();
	}

	if (*GameState == 6)
	{
		*pFogColor = FogColor;

		float time = ForceTime < 0 ? tod->CurrentTimeOfDay : ForceTime;

		SkyBrightness = std::lerp(Night.Sky, Morning.Sky, time);
		*SkyCarReflection = SkyBrightness * 0.2f;
		*SkyRoadReflection = SkyBrightness * 0.6f;
		WindowBrightness = std::lerp(Night.Vertex2, Morning.Vertex2, time);
		VertexBrightness = std::lerp(Night.Vertex3, Morning.Vertex3, time);
		*WorldBrightness = std::lerp(Night.Vertex1, Morning.Vertex1, time);
		*CarBrightness = std::lerp(Night.Car, Morning.Car, time);
		*FogFallof = std::lerp(0.0002579985012, 0.0001379985012, time);

		if (MoonRotation)
		{
			float skyTime = tod->UpdateDirection == 1 ? 1.0f - time : time;
			*SkyRotation = skyTime * 65535.0f;
		}

		if (LampFlareBackup < 0)
		{
			LampFlareBackup = *LampFlareSize;
		}

		*LampFlareSize = time > LampFlareThreshold ? 0 : LampFlareBackup;

		if (FilterTransition)
		{
			FilterLerp = time * time;
		}
	}

	if (*GameState == 3)
	{
		*CarBrightness = FrontEnd.Car;
		*WorldBrightness = 1.5f;
		WindowBrightness = 1.0f;
		VertexBrightness = 1.0f;
		*FogFallof = 0.0002579985012;
		*SkyRotation = 0;
	}
}

bool __fastcall DALOptions_GetTimeOfDay(void*, void*, float* val)
{
	*val = eTimeOfDayLighting::Instance()->CurrentTimeOfDay;
	return 1;
}

bool __fastcall DALOptions_SetTimeOfDay(void*, void*, float val)
{
	eTimeOfDayLighting::Instance()->CurrentTimeOfDay = val;
	return 1;
}

void __declspec(naked) DALOptions_GetFloat_Case_5004()
{
	_asm
	{
		mov eax, dword ptr ds : [esp + 0x10]
		push eax
		mov ecx, esi
		call DALOptions_GetTimeOfDay
		pop edi
		pop esi
		retn 0x14
	}
}

void __declspec(naked) DALOptions_SetFloat_Case_5004()
{
	_asm
	{
		mov eax, dword ptr ds : [esp + 0x10]
		push eax
		mov ecx, esi
		call DALOptions_SetTimeOfDay
		pop edi
		pop esi
		retn 0x14
	}
}

void __declspec(naked) FEOptionsScreen_AddDALOption_Case_5004_hook()
{
	_asm
	{
		// Set new values
		push 1
		push 0x3c23d70a // 0.01f, Step Size
		push 0x3F800000 // 1.0f, Max Value
		push 0 // Min Value

		// Jump back to create the option
		push 0x5C1E0D
		retn
	}
}

void Init()
{
	InitConfig();

	injector::MakeCALL(0x0072EC94, Update, true);

	injector::WriteMemory(0x0072E5E1, &SkyBrightness, true);
	injector::WriteMemory(0x007497BB, &WindowBrightness, true);
	injector::WriteMemory(0x00748A71, &VertexBrightness, true);

	// Disable fog falloff calculation
	injector::WriteMemory<BYTE>(0x007D2305, 0x1C, true);

	// Disable region color fog
	injector::MakeNOP(0x007D2336, 3, true);

	if (MoonRotation)
	{
		injector::MakeNOP(0x007AFCEA, 17, true);
		injector::MakeNOP(0x007AFD04, 3, true);
		injector::MakeNOP(0x007AFD0B, 7, true);
	}

	if (FilterTransition)
	{
		injector::WriteMemory(0x0071D702, Filters + Night.Filter, true);
		injector::WriteMemory(0x0071D747, Filters + Morning.Filter, true);
		injector::WriteMemory(0x007C8696, &FilterLerp, true);
		injector::WriteMemory(0x007C8686, 0x548B0CEB, true);
	}

	if (TimeSetting)
	{
		// Make time to go all the way down to 0
		injector::WriteMemory(0x007F109E, 0, true);
		injector::MakeNOP(0x007F1098, 2, true);

		// DALOptions hook
		injector::WriteMemory(0x4B3DA0, &DALOptions_GetFloat_Case_5004, true); // DALOptions::GetFloat jumptable
		injector::WriteMemory(0x4B3F18, &DALOptions_SetFloat_Case_5004, true); // DALOptions::SetFloat jumptable

		// Video Options (Basic): Replace Resolution with Time of Day
		injector::WriteMemory<DWORD>(0x5C6916, 5004, true); // FEOptionsStateManager::SetupPCVideoBasic (5004 = "Time of Night")

		// Adjust slider min max and step values
		injector::MakeJMP(0x5C1F79, FEOptionsScreen_AddDALOption_Case_5004_hook, true); // FeOptionsScreen::AddDalOption
	}
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
		IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)(base);
		IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);

		if ((base + nt->OptionalHeader.AddressOfEntryPoint + (0x400000 - base)) == 0x87E926) // Check if .exe file is compatible - Thanks to thelink2012 and MWisBest
		{
			Init();
		}
		else
		{
			MessageBoxA(NULL, "This .exe is not supported.\nPlease use v1.4 English nfsc.exe (6,88 MB (7.217.152 bytes)).", "Weather", MB_ICONERROR);
			return FALSE;
		}
	}

	return TRUE;
}