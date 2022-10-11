/**
 *  Copyright 2022 Thomas "twevs" Evans
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 *  later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along with this program. If not, see
 *  <https://www.gnu.org/licenses/>. 
 */

#include <cstdint>
#include <windows.h>
#include <TlHelp32.h>
#include <tchar.h>
#include <cstdio>
#include <conio.h>

void printError(const TCHAR* message);
UINT64 fnv1a(BYTE* data, size_t len);

int main()
{
	/**
	 * Find MISE process.
	 */

	// Take process snapshot.
	HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (processSnapshot == INVALID_HANDLE_VALUE)
	{
		printError(_T("Process snapshot"));
		return -1;
	}
	printf("Process snapshot taken.\n");
	printf("Will now iterate through processes to find MISE ...\n");

	// Iterate through processes until we find MISE process.
	PROCESSENTRY32 processEntry;
	processEntry.dwSize = sizeof(PROCESSENTRY32);
	BOOL foundProcessEntry = Process32First(processSnapshot, &processEntry);
	if (!foundProcessEntry)
	{
		printError(_T("Process entry iteration"));
		return -1;
	}

	while (foundProcessEntry)
	{
		if (wcscmp(processEntry.szExeFile, L"MISE.exe") == 0)
		{
			printf("Found MISE process.\n");
			break;
		}

		foundProcessEntry = Process32Next(processSnapshot, &processEntry);
		if (!foundProcessEntry)
		{
			printError(_T("Process entry iteration"));
			return -1;
		}
	}

	/**
	 * Open MISE process.
	 */

	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processEntry.th32ProcessID);
	if (!process)
	{
		printError(_T("Opening of process"));
		return -1;
	}
	printf("Opened process.\n");

	/**
	 * Find D3D9 module.
	 */

	// Take module snapshot.
	HANDLE moduleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
	                                                 processEntry.th32ProcessID);
	if (moduleSnapshot == INVALID_HANDLE_VALUE)
	{
		printError(_T("Module snapshot"));
		return -1;
	}
	printf("Module snapshot taken.\n");
	printf("Will now iterate through modules to find D3D9 ...\n");

	// Iterate through modules until we find D3D9 module.
	MODULEENTRY32 moduleEntry;
	moduleEntry.dwSize = sizeof(MODULEENTRY32);
	BOOL foundModuleEntry = Module32First(moduleSnapshot, &moduleEntry);
	if (!foundModuleEntry)
	{
		printError(_T("Module entry iteration"));
		return -1;
	}

	BYTE* d3d9PayloadAddress = nullptr;
	while (foundModuleEntry)
	{
		if (wcscmp(moduleEntry.szModule, L"d3d9.dll") == 0)
		{
			printf("Found D3D9 module @ address: 0x%p.\n", moduleEntry.modBaseAddr);
			printf("Module path: %ls.\n", moduleEntry.szExePath);
			d3d9PayloadAddress = moduleEntry.modBaseAddr + 0x46E66;
			break;
		}

		foundModuleEntry = Module32Next(moduleSnapshot, &moduleEntry);
		if (!foundModuleEntry)
		{
			printError(_T("Process entry iteration"));
			return -1;
		}
	}

	if (!d3d9PayloadAddress)
	{
		printf("Failed to find D3D9 module.\n");
		return -1;
	}

	/**
	 * Write pause payload to main module.
	 */

	LPVOID pausePayloadAddress = (LPVOID)0x402236;
	constexpr BYTE pausePayload[] =
	{
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
		0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
	};
	constexpr size_t pausePayloadSize = sizeof(pausePayload);

	// Verify integrity of destination.
	BYTE pauseCode[pausePayloadSize];
	if (!ReadProcessMemory(process, pausePayloadAddress, pauseCode, pausePayloadSize, nullptr))
	{
		printError(_T("Reading of pause code"));
		return -1;
	}
	constexpr UINT64 pauseHash = 11139880201765726062;
	UINT64 pauseCodeHash = fnv1a(pauseCode, pausePayloadSize);
	if (pauseCodeHash != pauseHash)
	{
		printf("Unable to verify integrity of destination of pause payload; aborting.\n");
		return -1;
	}

	if (!WriteProcessMemory(process, pausePayloadAddress, pausePayload, pausePayloadSize, nullptr))
	{
		printError(_T("Writing of pause payload"));
		return -1;
	}
	printf("Successfully wrote pause payload to main module.\n");

	/**
	 * Write rendering and window behaviour payload to D3D9 module.
	 */

	constexpr BYTE d3d9Payload[] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
	constexpr size_t d3d9PayloadSize = sizeof(d3d9Payload);

	// Verify integrity of destination.
	BYTE d3d9Code[d3d9PayloadSize];
	if (!ReadProcessMemory(process, d3d9PayloadAddress, d3d9Code, d3d9PayloadSize, nullptr))
	{
		printError(_T("Reading of D3D9 code"));
		return -1;
	}
	constexpr UINT64 d3d9Hash = 3090705834681805788;
	UINT64 d3d9CodeHash = fnv1a(d3d9Code, d3d9PayloadSize);
	if (d3d9CodeHash != d3d9Hash)
	{
		printf("Unable to verify integrity of destination of D3D9 payload; aborting.\n");
		return -1;
	}
	if (!WriteProcessMemory(process, d3d9PayloadAddress, d3d9Payload, d3d9PayloadSize, nullptr))
	{
		printError(_T("Writing of D3D9 payload"));
		return -1;
	}
	printf("Successfully wrote rendering and window behaviour payload to D3D9 module.\n");

	printf("You may now return to playing the game.\n");
	printf("Press any key to close this window.\n");
	_getch();
	return 0;
}

void printError(const TCHAR* message)
{
	DWORD lastError = GetLastError();
	TCHAR errorMessage[256];
	DWORD messageSize = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, lastError,
	                                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage,
	                                  sizeof(errorMessage) / sizeof(TCHAR), nullptr);

	if (messageSize == 0)
	{
		printf("Error: %ls.\n", message);
	}
	else
	{
		_tprintf(_T("%s failed: %s\n"), message, errorMessage);
	}

	printf("Press any key to close this window.\n");
	_getch();
}

UINT64 fnv1a(BYTE* data, size_t len)
{
	UINT64 hash = 14695981039346656037;
	UINT64 fnvPrime = 1099511628211;

	BYTE* currentByte = data;
	BYTE* end = data + len;
	while (currentByte < end)
	{
		hash ^= *currentByte;
		hash *= fnvPrime;
		++currentByte;
	}

	return hash;
}
