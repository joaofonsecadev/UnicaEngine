// 2021-2022 Copyright joaofonseca.dev, All Rights Reserved.

#include "TimeManager.h"

#include <ratio>
#include <chrono>

#include "fmt/color.h"

float TimeManager::m_DeltaTimeMillis;
float TimeManager::m_FrameWorkDuration;
float TimeManager::m_FrameSleepDuration;

void TimeManager::Init()
{
	if (std::chrono::steady_clock::period::den != std::nano::den)
	{
		UNICA_LOG(Fatal, "LogTimeManager", "HighResClock unit is not nanoseconds");
	}
}

void TimeManager::Tick()
{
    CalculateLastFrameTime();
    fmt::print(fg(fmt::color::light_gray), "\33[2K\rWorkFrameTime: {:.2f}; SleepFrameTime: {:.2f}; TotalFrameTime: {:.2f}",
        m_FrameWorkDuration, m_FrameSleepDuration, m_FrameWorkDuration + m_FrameSleepDuration);
}

void TimeManager::CalculateLastFrameTime()
{
    std::chrono::time_point CurrentFrameTime = std::chrono::steady_clock::now();
    std::chrono::nanoseconds DeltaTime = CurrentFrameTime - m_LastFrameTime;

    m_DeltaTimeMillis = DeltaTime.count() / 1'000'000.f;

    m_LastFrameTime = CurrentFrameTime;
}
