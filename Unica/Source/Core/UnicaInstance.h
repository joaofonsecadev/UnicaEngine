// 2022-2023 Copyright joaofonseca.dev, All Rights Reserved.

#pragma once

#include <filesystem>

#include "UnicaMinimal.h"
#include "Subsystem/SubsystemManager.h"

class UnicaInstance
{
public:
    void Init();
    void Tick();
    void Shutdown();

    static bool HasRequestedExit() { return m_bHasRequestedExit; }
    static void RequestExit() { m_bHasRequestedExit = true; }
    
    static std::filesystem::path GetProjectRootDirectory() { return m_ProjectRootDirectory; }
    static void SetProjectRootDirectory(char* SystemStyledExecutableDirectory);

private:
    void TickLogic();

    void Sleep(const uint64 NextFrameTimeTargetCount);
    
    std::unique_ptr<SubsystemManager> m_SubsystemManager;
    
    static bool m_bHasRequestedExit;
    static std::filesystem::path m_ProjectRootDirectory;
};
