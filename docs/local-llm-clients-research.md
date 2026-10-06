# Local LLM clients: mobile apps and Claude Code–style desktop agents

Research date: October 2026.

## 1. "Why is there no ChatGPT/Open WebUI-style mobile app for local models?"

There are several. They just aren't well known, because:

- **No money in it.** A client for your own server can't sell tokens or a subscription. These apps
  are mostly made by one person, as open source or a cheap one-time purchase, so they get no marketing.
- **Networking is the hard part, not the chat screen.** Ollama listens on `127.0.0.1` unless you set
  `OLLAMA_HOST=0.0.0.0`. Outside your Wi-Fi you need Tailscale, a VPN or a reverse proxy. iOS also
  blocks plain `http://` and asks for "Local Network" permission. A beginner gets stuck on these
  steps, whichever app they use.
- **App store rules.** Apple dislikes apps that do nothing without an outside server, and the
  plain-HTTP rule makes this worse. Some of these apps were rejected or pulled.
- **Open WebUI is already usable on a phone.** It runs as a PWA (Add to Home Screen), so a lot of
  people never needed a native app.

## 2. Mobile apps that exist today

| App | Platform | Backend | Shortcomings |
|---|---|---|---|
| **Conduit** (Open WebUI client) | iOS, Android | Open WebUI | You need an Open WebUI server, not just Ollama. Paid on iOS. Made by the community, not the Open WebUI team. |
| **Open WebUI PWA** | Any browser | Open WebUI | Not native: weak background streaming, no share sheet, and voice depends on the browser. |
| **Enchanted** | iOS, macOS | Ollama | Ollama only. Updates are slow. No RAG or tools. Can't reach your server from outside without a VPN. |
| **Reins** | iOS, Android, desktop | Ollama | Ollama only. Basic features: no web search, no RAG, no agents. |
| **Chatbox** | iOS, Android, desktop | Ollama, OpenAI-compatible, cloud | Mainly built for cloud APIs, so local setup is manual. Some features are paid. |
| **Invoke** | iOS | Ollama, LM Studio | Works on your Wi-Fi only. Small feature set. |
| **Ollama Connect** | iOS | Ollama | Ollama only. New app made by one developer. |
| **Off Grid** | Android | Ollama (finds it on the network automatically) | New. Little track record. Wi-Fi only unless you set up a VPN. |
| **LMSA** | Android | LM Studio, Ollama | Small project. Ads, or a paid tier. |

Shortcomings almost all of them share:

- No real agents or tools: no MCP, web search, code running or memory like ChatGPT.
- No easy remote access.
- Weak or missing voice mode.
- Chat history stays on the device, so it doesn't sync with the desktop.
- Each one usually depends on a single developer.

**Recommendation:** run Open WebUI next to Ollama, reach it through Tailscale, and use **Conduit**
on the phone. This is the closest thing to ChatGPT, with RAG, tools, voice and history saved on the
server. If you only want plain chat, use **Enchanted** (iOS) or **Reins** (Android).

## 3. Desktop agents like Claude Code that work with a local Ollama

| Tool | Type | How it uses Ollama | Notes |
|---|---|---|---|
| **Claude Code** itself | CLI | Ollama v0.14 and later supports the Anthropic Messages API. Run `ollama launch claude` (v0.15+), or set `ANTHROPIC_BASE_URL=http://localhost:11434` and `ANTHROPIC_AUTH_TOKEN=ollama`. | The real Claude Code features: tools, subagents, MCP, hooks. Its long system prompt and many tools need a model with a 64k+ context and strong tool calling. |
| **OpenCode** | CLI/TUI, plus a desktop app | Native Ollama/OpenAI-compatible provider, or `ollama launch opencode` | The most popular open-source copy of Claude Code. You can switch models in the middle of a session. |
| **OpenAI Codex CLI** | CLI | `--oss` / Ollama provider | Runs commands in a sandbox. Tuned for gpt-oss. |
| **Goose** (Block) | CLI and desktop app | Ollama, or its own llama.cpp | Good desktop app. Supports MCP. |
| **Aider** | CLI | `ollama_chat/<model>` | Edits through git diffs, so it needs less tool calling. Copes well with weaker models. |
| **Cline / Roo Code / Continue** | VS Code extensions | Ollama provider | Agents inside the editor. Cline shows each step and asks you to approve it. |

Shortcomings with local models:

- **Tool-calling quality** is the main problem. Small models (under about 14B) make bad JSON, call
  tools over and over, or stop partway through a task.
- **Context size.** Ollama's default `num_ctx` is small. Raise it, or the agent's system prompt gets
  cut off without any warning.
- **Speed.** Agents send long prompts many times, so prompt processing on consumer GPUs or Macs is slow.

Practical setup:

```bash
# models that handle tool calling reasonably well
ollama pull qwen3-coder:30b        # or gpt-oss:20b, devstral, glm-4.x
# make sure context is large enough (e.g. via Modelfile: PARAMETER num_ctx 65536)
ollama launch claude               # Claude Code on local models
# or
ollama launch opencode
```

**Recommendation:** use **Claude Code via `ollama launch claude`** if you want the same experience
as Claude Code. Use **OpenCode** if you want a fully open-source tool, and **Goose** if you want a GUI.

## Sources

- https://docs.ollama.com/api/anthropic-compatibility
- https://registry.ollama.ai/blog/claude
- https://dev.classmethod.jp/en/articles/claude-code-ollama-local/
- https://www.kdnuggets.com/pairing-claude-code-with-local-models
- https://simeononsecurity.com/articles/claude-cowork-claude-code-local-models-2026/
- https://devtoollab.com/blog/open-source-alternatives-claude-code
- https://apps.apple.com/us/app/conduit-open-webui-client/id6749840287
- https://docs.openwebui.com/getting-started/open-webui-as-app.txt
- https://apps.apple.com/us/app/6739738501 (Reins)
- https://sourceforge.net/mirror/enchanted/profile/
- https://apps.apple.com/app/id6749133236 (Invoke)
- https://mwm.ai/apps/ollama-connect/6769891596
- https://medium.com/@mohammed.ali.chherawalla/how-to-use-ollama-from-your-android-phone-in-2026-auto-discovery-zero-setup-96ce58fb8dac
- https://lmsa.app/blog/chat-ollama-lm-studio-smartphone/
- https://www.agenticwire.news/article/best-ollama-gui
- https://mintlify.com/block/goose/troubleshooting/known-issues
