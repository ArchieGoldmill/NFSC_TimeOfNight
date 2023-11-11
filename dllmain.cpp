#include <Windows.h>
#include "Injector/injector.hpp"
#include "IniReader/IniReader.h"

float lerp(float a, float b, float f)
{
	return a * (1.0 - f) + (b * f);
}

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

unsigned int FogColor = 0;

struct Presset
{
	float Vertex1;
	float Vertex2;
	float Vertex3;
	float Sky;
	float Car;
} Morning, Night, FrontEnd;

float ForceTime = -1.0f;
float LampFlareThreshold = 0.0f;
float LampFlareBackup = -1.0f;
bool MoonRotation = false;
bool LiveReload = false;

void InitPresset(CIniReader& iniReader, Presset& presset, const char* name)
{
	presset.Car = iniReader.ReadFloat(name, "Car", 1.0f);
	presset.Sky = iniReader.ReadFloat(name, "Sky", 1.0f);
	presset.Vertex1 = iniReader.ReadFloat(name, "Vertex1", 1.0f);
	presset.Vertex2 = iniReader.ReadFloat(name, "Vertex2", 1.0f);
	presset.Vertex3 = iniReader.ReadFloat(name, "Vertex3", 1.0f);
}

void InitConfig()
{
	CIniReader iniReader("NFSCWeather.ini");

	InitPresset(iniReader, Morning, "MORNING");
	InitPresset(iniReader, Night, "NIGHT");
	InitPresset(iniReader, FrontEnd, "FRONTEND");

	ForceTime = iniReader.ReadFloat("GENERAL", "ForceTime", -1.0f);

	LampFlareThreshold = iniReader.ReadFloat("GENERAL", "LampFlareThreshold", 0.85f);

	MoonRotation = iniReader.ReadInteger("GENERAL", "MoonRotation", 0) == 1;

	LiveReload = iniReader.ReadInteger("GENERAL", "LiveReload", 0) == 1;

	FogColor = iniReader.ReadUInteger("GENERAL", "FogColor", 0);
}

float SkyBrightness = 1;
float WindowBrightness = 1;
float VertexBrightness = 1;
auto CarBrightness = (float*)0x009EA968;
auto WorldBrightness = (float*)0x00A6C204;
auto FogFallof = (float*)0x00B74228;
auto GameState = (int*)0x00A99BBC;
auto SkyRotation = (unsigned short*)0x00B77B78;
auto pFogColor = (int*)0x00B74234;
auto LampFlareSize = (float*)0x00A6BC14;

void Update()
{
	if (LiveReload)
	{
		InitConfig();
	}

	if (*GameState == 6)
	{
		*pFogColor = FogColor;

		auto tod = eTimeOfDayLighting::Instance();
		float time = ForceTime < 0 ? tod->CurrentTimeOfDay : ForceTime;

		SkyBrightness = lerp(Night.Sky, Morning.Sky, time);
		WindowBrightness = lerp(Night.Vertex2, Morning.Vertex2, time);
		VertexBrightness = lerp(Night.Vertex3, Morning.Vertex3, time);
		*WorldBrightness = lerp(Night.Vertex1, Morning.Vertex1, time);
		*CarBrightness = lerp(Night.Car, Morning.Car, time);
		*FogFallof = lerp(0.0002579985012, 0.0001379985012, time);

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
	}

	if (*GameState == 3)
	{
		*CarBrightness = FrontEnd.Car;
		*WorldBrightness = 1.5;
		*FogFallof = 0.0002579985012;
		*SkyRotation = 0;
	}
}

void UpdateHook()
{
	__asm pushad;

	Update();

	__asm popad;
}

void Init()
{
	InitConfig();

	// Hope no one uses this nullsub
	injector::MakeCALL(0x0072E97E, UpdateHook, true);

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

	// Make time to go all the way down to 0
	injector::WriteMemory(0x007F109E, 0, true);
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