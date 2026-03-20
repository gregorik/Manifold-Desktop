# Contributing to Manifold Desktop

## Prerequisites

- **Windows 10** version 1903 or later (x64)
- **Visual Studio 2022** or later with:
  - Desktop development with C++ workload
  - Universal Windows Platform development workload
  - Windows 10 SDK (10.0.19041.0 or later)
- **Windows App SDK 1.8** -- installed via NuGet on first build

## Setup

```bash
git clone https://github.com/anthropic/manifold-desktop.git
cd manifold-desktop
msbuild ManifoldDesktop.vcxproj /p:Configuration=Debug /p:Platform=x64 /restore
```

Or open `ManifoldDesktop.sln` in Visual Studio, which restores NuGet packages automatically.

## Project Structure

```
ManifoldDesktop.vcxproj     Main project file
ManifoldDesktop.sln         Solution file
App.xaml / App.xaml.cpp      WinUI 3 application entry point
MainWindow.xaml / .cpp / .h  Main window, WebView2 host, message dispatcher

Manifold.Core/
  Providers/                 IProvider interface + Gemini, OpenAI, Anthropic, OpenAICompat
  MCP/                       MCPClient, StdioTransport, SSETransport
  Plugins/                   IPlugin interface, PluginManager, PluginContext
  ProcessManager.*           ConPTY pseudo-console management
  TerminalEmulator.*         ANSI parser and character grid
  SettingsManager.*          JSON settings persistence
  SessionManager.*           Chat session storage and search
  PromptManager.*            Prompt library storage
  CredentialManager.*        Windows Credential Manager (DPAPI) wrapper

frontend/
  index.html                 Single-page shell
  app.js                     Main orchestrator
  components/                UI components (tab-bar, chat-tab, terminal-tab, etc.)
  services/                  Bridge, settings store, provider API, pricing
  styles/                    CSS files
  vendor/                    Third-party JS (marked.js, highlight.js)
```

## Code Style

### C++

- **Standard**: C++20. Use `std::jthread` for background work, `std::optional` where appropriate.
- **Threading**: Streaming chat runs on `std::jthread`. Always marshal UI updates back to the dispatcher queue.
- **JSON**: Use `nlohmann::json` for all serialization. Prefer ADL `to_json`/`from_json` overloads for structured types.
- **Error handling**: Use return values and `std::optional` over exceptions in performance-sensitive code. Exceptions are acceptable for truly exceptional conditions.
- **Naming**: PascalCase for classes and public methods, camelCase for local variables, `m_` prefix for member variables.
- **Headers**: Use `#pragma once`. Keep includes minimal.

### JavaScript

- **Modules**: Vanilla ES6 modules. No npm, no bundler, no framework.
- **Component pattern**: Each component exports `create()` which builds DOM, attaches listeners, and returns a public interface.
- **Naming**: camelCase for functions and variables, PascalCase for classes (rare).
- **No globals**: All state is module-scoped or passed explicitly.

## Pull Request Guidelines

1. **One feature or fix per PR.** Keep changes focused and reviewable.
2. **Describe what and why** in the PR description. Include screenshots for UI changes.
3. **Test on both Debug and Release** (`x64` platform) before submitting.
4. **No new dependencies** without discussion. The frontend intentionally has zero npm dependencies.
5. **Match existing patterns.** Look at nearby code for conventions before introducing new ones.

## Reporting Issues

Use GitHub Issues. Include:

- Windows version (e.g., Windows 11 23H2)
- Steps to reproduce
- Expected vs. actual behavior
- Any relevant error messages or logs

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](../LICENSE).
