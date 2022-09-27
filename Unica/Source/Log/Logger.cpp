// Copyright joaofonseca.dev, All Rights Reserved.

#include "Logger.h"

#include "fmt/printf.h"
#include "fmt/color.h"

#include "UnicaInstance.h"

void Logger::Log(LogLevel LogLevel, const std::string& LogCategory, const std::string& LogText)
{
    std::string LogLevelInfoName;
    fmt::text_style LogLevelTextStyle = fg(fmt::color::white);

    switch (LogLevel)
    {
        case ::Log:
            break;
        case Warning:
            LogLevelInfoName = "[WARNG] ";
            LogLevelTextStyle = fg(fmt::color::yellow);
            break;
        case Error:
            LogLevelInfoName = "[ERROR] ";
            LogLevelTextStyle = fg(fmt::color::red);
            break;
        case Fatal:
            LogLevelInfoName = "[FATAL] ";
            LogLevelTextStyle = fg(fmt::color::dark_magenta) | fmt::emphasis::bold;
            break;
    }

    fmt::print(LogLevelTextStyle, "{}[{}] {}\n", LogLevelInfoName, LogCategory, LogText);

    if (LogLevel == Fatal)
    {
        UnicaInstance::RequestExit();
    }
}

