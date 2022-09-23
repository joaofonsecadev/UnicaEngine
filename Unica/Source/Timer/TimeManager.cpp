// Copyright joaofonseca.dev, All Rights Reserved.

#include "TimeManager.h"

#include <ratio>
#include <chrono>
#include "fmt/color.h"
#include "Log/Logger.h"

void TimeManager::Init()
{
    if (std::chrono::high_resolution_clock::period::den != std::nano::den)
    {
        UNICA_LOG(Fatal, "LogTimeManager", "HighResClock unit is not nanoseconds. Aborting execution");
    }
    m_LastFrameTimeNano = GetNanoSinceEpoch();
}

void TimeManager::Tick()
{
    CalculateLastFrameTime();
    fmt::print(fg(fmt::color::white),
               "\33[2K\r[LogTimeManager] I'm being ticked at {:.0f} fps", 1 / GetDeltaTimeInSeconds());
}

uint64 TimeManager::GetNanoSinceEpoch()
{
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

void TimeManager::CalculateLastFrameTime()
{
    auto CurrentFrameTime = GetNanoSinceEpoch();
    m_DeltaTimeNano = CurrentFrameTime - m_LastFrameTimeNano;
    m_LastFrameTimeNano = CurrentFrameTime;
}

float TimeManager::GetDeltaTimeInSeconds()
{
    return (float)m_DeltaTimeNano / 1'000'000'000.f;
}
