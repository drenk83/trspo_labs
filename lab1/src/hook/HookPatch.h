#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>

// Ставит inline-хук на первые patchSize байт функции target
// Оригинальные байты сохраняются и выполняются в трамплине,
// поэтому перехваченная функция может вызвать оригинал
class HookPatch {
public:
    // target — начало функции, hook — обработчик, patchSize — размер патча
    HookPatch(void* target, void* hook, size_t patchSize);
    // Снимает хук, если он был установлен
    ~HookPatch();

    HookPatch(const HookPatch&) = delete;
    HookPatch& operator=(const HookPatch&) = delete;

    // Устанавливает хук; при желании возвращает адрес трамплина
    bool Install(void** trampolineOut = nullptr);
    // Возвращает оригинальные байты и освобождает трамплин
    bool Remove();

    bool IsInstalled() const { return installed_; }
    bool UsesAbsoluteJump() const { return absoluteJump_; }
    size_t PatchSize() const { return patchSize_; }
    void* Trampoline() const { return trampoline_; }

private:
    // Собирает байты перехода E9 или FF 25
    bool BuildNewBytes();
    // Выделяет трамплин и копирует туда оригинальные инструкции
    bool AllocateTrampoline();
    // Освобождает память трамплина
    void FreeTrampoline();

    void* target_;
    void* hook_;
    size_t patchSize_;

    BYTE oldBytes_[32];
    BYTE newBytes_[32];
    void* trampoline_;
    DWORD oldProtect_;
    bool installed_;
    bool absoluteJump_;
};
