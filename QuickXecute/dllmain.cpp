#include <iostream>
#include <Windows.h>
#include <thread>
#include <string>
#include <sstream>

typedef int(__fastcall* deserialize_t)(std::uintptr_t L, const char* chunk_name, const char* bytecode, int bytecode_size, int env);
deserialize_t deserialize;

typedef int(__cdecl* spawn_t)(std::uintptr_t L);
spawn_t spawn;

constexpr std::uintptr_t SCRIPTCONTEXT_OFFSET = 0x2977B34;
constexpr std::uintptr_t DESERIALIZE_OFFSET = 0x15BAFF0;
constexpr std::uintptr_t SPAWN_OFFSET = 0x39BC10;
constexpr std::uintptr_t MIDDLE_MAN_JMP_OFFSET = 0x2133E0; // Must be 'jmp esi' inside .text or else Roblox retcheck will trigger.
constexpr std::uintptr_t STATE_OFFSET = 127;

constexpr const char* CHUNK_NAME = "=Xecute";
constexpr const char* BYTECODE = "01 02 05 70 72 69 6E 74 0B 68 65 6C 6C 6F 20 77 6F 72 6C 64 01 02 00 00 01 06 A3 00 00 00 A4 00 01 00 00 00 00 40 6F 01 02 00 9F 00 02 00 82 00 00 00 03 03 01 04 00 00 00 40 03 02 00 00 01 18 00 00 00 00 00 00 01 00 00 00 00 00";

std::uintptr_t base_address;
std::uintptr_t L;

std::uintptr_t scan(std::uintptr_t vftable_address) {
    SYSTEM_INFO system_info;
    MEMORY_BASIC_INFORMATION memory_info;
    GetSystemInfo(&system_info);

    std::uintptr_t page_size = system_info.dwPageSize;
    std::uintptr_t* buffer = new std::uintptr_t[page_size];

    std::uintptr_t target_address_memory_location = reinterpret_cast<std::uintptr_t>(&vftable_address);

    for (std::uintptr_t addr = base_address; addr < 0x7FFFFFFF; addr += page_size) {
        VirtualQuery(reinterpret_cast<LPCVOID>(addr), &memory_info, page_size);
        if (memory_info.Protect == PAGE_READWRITE) {
            std::memcpy(buffer, reinterpret_cast<void*>(addr), page_size);
            for (std::uintptr_t i = 0; i <= page_size / 4; i++) {
                if (buffer[i] == vftable_address) {
                    std::uintptr_t address = static_cast<std::uintptr_t>(addr + (i * 4));
                    delete[] buffer;
                    return address;
                }
            }
        }
    }

    delete[] buffer;
    return 0;
}

void deserialize_wrapper(std::uintptr_t L, const char* chunk_name, const char* bytecode, std::size_t bytecode_length) {
    std::uintptr_t middle_man_jmp = base_address + MIDDLE_MAN_JMP_OFFSET;
    __asm {
        push esi
        mov esi, deserialize_finish

        mov ecx, L
        mov edx, chunk_name
        push 0
        push bytecode_length
        push bytecode

        push [middle_man_jmp]
        jmp deserialize

        deserialize_finish: // It will return here after the function call.
        add esp, 0xC
        pop esi
    }
}

void spawn_wrapper(std::uintptr_t L) {
    std::uintptr_t middle_man_jmp = base_address + MIDDLE_MAN_JMP_OFFSET;
    __asm {
        push esi
        mov esi, spawn_finish

        push L

        push [middle_man_jmp]
        jmp spawn

        spawn_finish: // It will return here after the function call.
        add esp, 0x4
        pop esi
    }
}

std::string hex_to_string(const std::string& str) {
    std::string res;
    std::istringstream hex_bytecode(str);
    std::uint32_t character;
    while (hex_bytecode >> std::hex >> character) {
        res.push_back(character);
    }
    return res;
}

int main() {
    base_address = reinterpret_cast<std::uintptr_t>(GetModuleHandleA("RobloxPlayerBeta.exe"));
    deserialize = reinterpret_cast<deserialize_t>(base_address + DESERIALIZE_OFFSET);
    spawn = reinterpret_cast<spawn_t>(base_address + SPAWN_OFFSET);

    std::string bytecode = hex_to_string(std::string(BYTECODE));
    const char* bytecode_cstr = bytecode.c_str();
    std::size_t bytecode_length = bytecode.length();

    std::uintptr_t scriptcontext = scan(base_address + SCRIPTCONTEXT_OFFSET);
    L = reinterpret_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t*>(scriptcontext) + STATE_OFFSET)
        ^ reinterpret_cast<std::uintptr_t*>(scriptcontext)[STATE_OFFSET];

    deserialize_wrapper(L, CHUNK_NAME, bytecode_cstr, bytecode_length);
    spawn_wrapper(L);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            std::thread(main).detach();
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
