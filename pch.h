#pragma once

// Prevent Windows.h macros from conflicting with std::min() and std::max()
#define NOMINMAX

// Windows API - need full headers for WinRT COM support
#include <windows.h>
#include <wincon.h>
#include <Unknwn.h>

// Undefine problematic macros from windows.h before WinRT headers
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

// WinRT base and foundation
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

// WinUI 3 headers
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Documents.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Dispatching.h>

// Windows namespace headers needed for UI
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>

// Shell API for folder paths
#include <ShlObj.h>

// C++ Standard Library
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>
