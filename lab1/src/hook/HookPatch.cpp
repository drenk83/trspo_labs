#include "HookPatch.h"

#include <cstring>

namespace {

constexpr size_t kRelativeJumpSize = 5;
constexpr size_t kAbsoluteJumpSize = 14;
constexpr size_t kTrampolineTailSize = 14;

}  // namespace

// Запоминает адреса и размер патча
HookPatch::HookPatch(void* target, void* hook, size_t patchSize)
    : target_(target),
      hook_(hook),
      patchSize_(patchSize),
      trampoline_(nullptr),
      oldProtect_(0),
      installed_(false),
      absoluteJump_(false) {
    ZeroMemory(oldBytes_, sizeof(oldBytes_));
    ZeroMemory(newBytes_, sizeof(newBytes_));
}

// Снимает хук при уничтожении объекта
HookPatch::~HookPatch() {
    Remove();
}

// Выбирает относительный или абсолютный переход и заполняет newBytes_
bool HookPatch::BuildNewBytes() {
    if (target_ == nullptr || hook_ == nullptr) {
        return false;
    }
    if (patchSize_ < kRelativeJumpSize || patchSize_ > sizeof(newBytes_)) {
        return false;
    }

    ZeroMemory(newBytes_, sizeof(newBytes_));

    const intptr_t delta = reinterpret_cast<BYTE*>(hook_) -
                           (reinterpret_cast<BYTE*>(target_) + kRelativeJumpSize);
    const int32_t relative = static_cast<int32_t>(delta);
    absoluteJump_ = static_cast<intptr_t>(relative) != delta;

    if (absoluteJump_) {
        if (patchSize_ < kAbsoluteJumpSize) {
            return false;
        }

        newBytes_[0] = 0xFF;
        newBytes_[1] = 0x25;
        // disp32 = 0: jmp qword ptr [rip+0]
        const uintptr_t hookAddress = reinterpret_cast<uintptr_t>(hook_);
        memcpy(&newBytes_[6], &hookAddress, sizeof(hookAddress));
        memset(&newBytes_[kAbsoluteJumpSize], 0x90, patchSize_ - kAbsoluteJumpSize);
        return true;
    }

    newBytes_[0] = 0xE9;
    memcpy(&newBytes_[1], &relative, sizeof(relative));
    memset(&newBytes_[kRelativeJumpSize], 0x90, patchSize_ - kRelativeJumpSize);
    return true;
}

// Создаёт трамплин: оригинальные инструкции и переход обратно
bool HookPatch::AllocateTrampoline() {
    trampoline_ = VirtualAlloc(nullptr, patchSize_ + kTrampolineTailSize,
                               MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (trampoline_ == nullptr) {
        return false;
    }

    BYTE* trampoline = static_cast<BYTE*>(trampoline_);
    memcpy(trampoline, oldBytes_, patchSize_);

    BYTE* jumpBack = trampoline + patchSize_;
    jumpBack[0] = 0xFF;
    jumpBack[1] = 0x25;
    ZeroMemory(&jumpBack[2], 4);

    const uintptr_t returnAddress = reinterpret_cast<uintptr_t>(target_) + patchSize_;
    memcpy(&jumpBack[6], &returnAddress, sizeof(returnAddress));
    return true;
}

// Освобождает память трамплина
void HookPatch::FreeTrampoline() {
    if (trampoline_ != nullptr) {
        VirtualFree(trampoline_, 0, MEM_RELEASE);
        trampoline_ = nullptr;
    }
}

// Сохраняет оригинал, ставит патч и отдаёт адрес трамплина
bool HookPatch::Install(void** trampolineOut) {
    if (installed_) {
        if (trampolineOut != nullptr) {
            *trampolineOut = trampoline_;
        }
        return true;
    }
    if (trampolineOut != nullptr) {
        *trampolineOut = nullptr;
    }
    if (!BuildNewBytes()) {
        return false;
    }

    // Сначала читаем оригинал и готовим трамплин, потом патчим код
    memcpy(oldBytes_, target_, patchSize_);
    if (!AllocateTrampoline()) {
        return false;
    }

    // Отдаём трамплин вызывающему до записи патча: хук не должен
    // увидеть nullptr, если target вызовут сразу после установки
    if (trampolineOut != nullptr) {
        *trampolineOut = trampoline_;
    }

    if (!VirtualProtect(target_, patchSize_, PAGE_EXECUTE_READWRITE, &oldProtect_)) {
        if (trampolineOut != nullptr) {
            *trampolineOut = nullptr;
        }
        FreeTrampoline();
        return false;
    }

    memcpy(target_, newBytes_, patchSize_);
    FlushInstructionCache(GetCurrentProcess(), target_, patchSize_);

    DWORD ignored = 0;
    VirtualProtect(target_, patchSize_, oldProtect_, &ignored);
    installed_ = true;
    return true;
}

// Возвращает оригинальные байты и освобождает трамплин
bool HookPatch::Remove() {
    if (!installed_) {
        return true;
    }

    DWORD currentProtect = 0;
    if (!VirtualProtect(target_, patchSize_, PAGE_EXECUTE_READWRITE, &currentProtect)) {
        return false;
    }

    memcpy(target_, oldBytes_, patchSize_);
    FlushInstructionCache(GetCurrentProcess(), target_, patchSize_);

    DWORD ignored = 0;
    VirtualProtect(target_, patchSize_, oldProtect_, &ignored);

    FreeTrampoline();
    installed_ = false;
    return true;
}
