<h1>Manifold Desktop</h1>
<p>Open-source multi-provider AI chat client for Windows.</p>
<p><img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT">
<img src="https://img.shields.io/badge/Platform-Windows%2010%2B-0078D4.svg" alt="Platform: Windows 10+">
<img src="https://img.shields.io/badge/Language-C%2B%2B20-00599C.svg" alt="Language: C++20"></p>
<p><img src="docs/screenshot.png" alt="Screenshot"></p>
<p><em>(Screenshot coming soon)</em></p>

Manifold Desktop is a native Windows desktop client that lets you chat with multiple AI providers through a single
  unified interface. It is experimental and not production-ready at this point. Its core value proposition: Instead of juggling separate tabs/apps for ChatGPT, Claude, Gemini, and local models, you use
  one app with one conversation history, one settings panel, and the ability to switch providers mid-conversation or
  compare them side-by-side.

<h2>Features</h2>
<ul>
<li><strong>Multi-provider support</strong> -- Gemini, OpenAI, Anthropic, Ollama, and any OpenAI-compatible endpoint</li>
<li><strong>Side-by-side model comparison</strong> -- stream responses from two models in parallel</li>
<li><strong>Integrated terminal</strong> -- full ConPTY pseudo-console with ANSI rendering</li>
<li><strong>MCP client</strong> -- Model Context Protocol support with stdio and SSE transports</li>
<li><strong>Prompt library</strong> -- save and reuse system prompts across conversations</li>
<li><strong>Session management</strong> -- save, load, search, export, and import chat sessions</li>
<li><strong>Markdown export</strong> -- export conversations as <code>.md</code> files</li>
<li><strong>Cost tracking</strong> -- per-conversation token usage and cost estimates</li>
<li><strong>Plugin system</strong> -- extend functionality via DLL plugins with virtual host registration</li>
<li><strong>Secure API key storage</strong> -- Windows Credential Manager (DPAPI) encryption</li>
<li><strong>Keyboard shortcuts</strong><ul>
<li><code>Ctrl+N</code> -- new chat</li>
<li><code>Ctrl+T</code> -- new terminal</li>
<li><code>Ctrl+W</code> -- close tab</li>
<li><code>Ctrl+F</code> -- search sessions</li>
<li><code>Ctrl+,</code> -- settings</li>
<li><code>Ctrl+K</code> -- focus sidebar search</li>
<li><code>Ctrl+1..9</code> -- switch tabs</li>
</ul>
</li>
</ul>
<h2>Quick Start</h2>
<h3>Prerequisites</h3>
<ul>
<li>Windows 10 version 1903 or later (x64)</li>
<li><a href="https://visualstudio.microsoft.com/">Visual Studio 2022</a> or later with:<ul>
<li><strong>Desktop development with C++</strong> workload</li>
<li><strong>Universal Windows Platform development</strong> workload</li>
<li>Windows 10 SDK (10.0.19041.0 or later)</li>
</ul>
</li>
<li><a href="https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/">Windows App SDK 1.8</a></li>
</ul>
<h3>Clone and build</h3>
<pre><code class="language-bash">git clone https://github.com/anthropic/manifold-desktop.git
cd manifold-desktop
</code></pre>
<p>Restore NuGet packages and build:</p>
<pre><code class="language-bash">msbuild ManifoldDesktop.vcxproj /p:Configuration=Release /p:Platform=x64 /restore
</code></pre>
<p>Run the output from <code>x64\Release\ManifoldDesktop\</code>.</p>
<h2>Build from Source</h2>
<h3>Debug build</h3>
<pre><code class="language-bash">msbuild ManifoldDesktop.vcxproj /p:Configuration=Debug /p:Platform=x64 /restore
</code></pre>
<h3>Release build</h3>
<pre><code class="language-bash">msbuild ManifoldDesktop.vcxproj /p:Configuration=Release /p:Platform=x64 /restore
</code></pre>
<p>The <code>/restore</code> flag triggers NuGet package restore automatically. If you prefer to restore separately:</p>
<pre><code class="language-bash">nuget restore ManifoldDesktop.sln
msbuild ManifoldDesktop.vcxproj /p:Configuration=Release /p:Platform=x64
</code></pre>
<p>You can also open <code>ManifoldDesktop.sln</code> in Visual Studio, which handles NuGet restore on build.</p>
<h2>Architecture</h2>
<p>Manifold Desktop is a two-process application:</p>
<ul>
<li><strong>C++/WinRT backend</strong> -- manages providers, terminal sessions, MCP connections, file I/O, and credential storage</li>
<li><strong>WebView2 frontend</strong> -- vanilla JavaScript (ES6 modules) with no framework or build step</li>
</ul>
<p>The two layers communicate via a JSON message bridge (<code>postMessage</code> / <code>OnWebMessage</code>). Each provider implements the <code>IProvider</code> interface and is registered at startup through <code>ProviderRegistry</code>.</p>
<p>See <a href="docs/architecture.md">docs/architecture.md</a> for a detailed breakdown.</p>
<h2>Extending</h2>
<h3>Adding a Provider</h3>
<ol>
<li>Create a class that implements <code>Manifold::Core::IProvider</code> (see <code>Manifold.Core/Providers/IProvider.h</code>).</li>
<li>Implement all virtual methods: <code>Id()</code>, <code>DisplayName()</code>, <code>ListModels()</code>, <code>SendChat()</code>, <code>StreamChat()</code>, and <code>ValidateKey()</code>.</li>
<li>Register an instance in <code>MainWindow</code>&#39;s constructor alongside the existing providers:</li>
</ol>
<pre><code class="language-cpp">m<em>providerRegistry.AddProvider(std::make</em>unique&lt;YourProvider&gt;());
</code></pre>
<ol start="4">
<li>Rebuild. The new provider will appear in the frontend&#39;s provider dropdown automatically.</li>
</ol>
<h3>Adding an MCP Server</h3>
<ol>
<li>Open Settings (<code>Ctrl+,</code>) and navigate to the MCP section.</li>
<li>Add a server with either:<ul>
<li><strong>Stdio transport</strong> -- provide the command and arguments (e.g., <code>npx -y @modelcontextprotocol/server-filesystem /path</code>)</li>
<li><strong>SSE transport</strong> -- provide the server URL (e.g., <code>http://localhost:8080/sse</code>)</li>
</ul>
</li>
<li>Enable the server. Manifold will connect and discover available tools, which are then offered to AI providers that support tool use.</li>
</ol>
<h2>Contributing</h2>
<p>See <a href="docs/contributing.md">docs/contributing.md</a> for setup instructions, code style, and PR guidelines.</p>
<h2>License</h2>
<p><a href="LICENSE">MIT</a></p>



Using C++20 backend, ES6 module frontend, nlohmann/json, ConPTY.
