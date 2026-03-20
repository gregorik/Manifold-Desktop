Manifold Desktop is a native Windows desktop client that lets you chat with multiple AI providers through a single
  unified interface. It is experimental and not production-ready at this point.

  Core value proposition: Instead of juggling separate tabs/apps for ChatGPT, Claude, Gemini, and local models, you use
  one app with one conversation history, one settings panel, and the ability to switch providers mid-conversation or
  compare them side-by-side.

  What it does:

  - Multi-provider chat — Talk to Gemini, OpenAI (GPT-4o), Anthropic (Claude), any OpenAI-compatible endpoint, or Ollama
   local models, all from the same window. Switch providers with a dropdown.
  - Side-by-side comparison — Send the same prompt to 2+ providers simultaneously and see responses stream in parallel
  columns. Useful for evaluating model quality, speed, and cost.
  - Cost tracking — Shows per-message token count and estimated USD cost. Tracks cumulative usage per provider in a
  dashboard.
  - Prompt library — Save and organize reusable prompts (user or system). Insert them from a dropdown in the input bar.
  - Session management — Conversations are persisted locally. Export as JSON or Markdown. Import sessions. Search across
   all conversations with Ctrl+F.
  - Built-in terminal — Embedded terminal tab using ConPTY pseudo-console, so you can run commands without leaving the
  app.
  - MCP support — Connect to Model Context Protocol servers (stdio or SSE transport) to give models access to external
  tools.
  - Plugin system — Extensible plugin architecture with virtual host registration.
  - Local-first & private — Native C++/WinRT app. API keys stored in Windows Credential Manager (DPAPI). All data stays
  on your machine.

  Tech stack: WinUI 3 + WebView2 shell, C++20 backend, ES6 module frontend, nlohmann/json, ConPTY.
