#!/usr/bin/env python3
"""ai_helper.py - Smart English -> Linux command translator, explainer & chatbot.
Live mode: Gemini (needs GEMINI_API_KEY + google-genai). Otherwise: offline rules."""
import contextlib
import io
import os
import re
import sys

MODEL = "gemini-3.5-flash-lite"

SMART_SYSTEM = (
    "You are the AI brain inside mysh, a custom C Linux shell running on Debian (xfce4-terminal). "
    "Understand casual, everyday, or misspelled English (including texting slang like 'cng clr', 'pls', 'u', 'pc specs').\n"
    "Decide if the user wants to RUN an action on their computer OR just CHAT / ask a question:\n"
    "- If they want to perform a computer/terminal action, reply strictly starting with 'CMD: ' followed by ONE raw command line.\n"
    "  Command rules:\n"
    "  1. Single line only, no markdown, no backticks, no explanation.\n"
    "  2. mysh supports multi-pipes (|), redirection (>, >>, <), background (&), and chaining (&&, ||, ;).\n"
    "  3. Built-in Avengers themes: if the user asks for Iron Man, Captain America/Cap, Hulk, or Thor look/theme, "
    "output: CMD: theme ironman, CMD: theme cap, CMD: theme hulk, or CMD: theme thor.\n"
    "  4. To reset theme or terminal colors back to normal, output: CMD: theme reset.\n"
    "  5. For any other custom text color (like orange, purple, pink), output: CMD: printf '\\033]10;#RRGGBB\\007'\n"
    "  6. For any other custom background color, output: CMD: printf '\\033]11;#RRGGBB\\007'\n"
    "  7. Built-in system specs: if the user asks for PC specs, CPU, RAM, memory, or uptime, output: CMD: sysinfo.\n"
    "  8. Built-in process list: if the user asks to see running processes with memory, output: CMD: procs.\n"
    "  9. Other built-ins available: cd [dir], explain <cmd>, history, jobs, help, chat, exit.\n"
    "- If they are greeting you, chatting, or asking a general/concept question (like 'hi', 'who are you', "
    "'what is a deadlock', 'what does ls do'), reply strictly starting with 'CHAT: ' followed by a short, "
    "friendly plain-text reply (1 to 4 sentences, no markdown stars or backticks).\n"
    "Request: "
)

EXPLAIN_SYSTEM = (
    "Explain what the following Linux command and its flags do in 2 to 3 simple, beginner-friendly sentences. "
    "Do not use markdown stars, hashes, or backticks. Warn clearly if the command deletes or modifies files.\n"
    "Command: "
)

CHAT_SYSTEM = (
    "You are mysh AI, a friendly, helpful assistant built into a custom Linux mini shell. "
    "Talk naturally like a helpful friend. Keep answers clear, concise, and easy to read in a terminal "
    "(avoid messy markdown symbols like ** or ###). Understand casual texting slang and typos effortlessly."
)


def strip_md(text):
    return re.sub(r"```[a-zA-Z]*", "", text or "").replace("`", "").strip()


def format_smart(text):
    raw = strip_md(text)
    if not raw:
        return ""
    if raw.upper().startswith("CHAT:"):
        return "CHAT: " + raw[5:].strip()
    if raw.upper().startswith("CMD:"):
        cmd = raw[4:].strip().splitlines()[0].strip()
        return "CMD: " + cmd
    return "CMD: " + raw.splitlines()[0].strip()


def live_smart(prompt):
    if not os.environ.get("GEMINI_API_KEY"):
        return None
    try:
        with contextlib.redirect_stderr(io.StringIO()):
            from google import genai
            client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])
            resp = client.models.generate_content(
                model=MODEL, contents=SMART_SYSTEM + prompt)
        return format_smart(resp.text) or None
    except Exception:
        return None


def live_explain(cmd):
    if not os.environ.get("GEMINI_API_KEY"):
        return None
    try:
        with contextlib.redirect_stderr(io.StringIO()):
            from google import genai
            client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])
            resp = client.models.generate_content(
                model=MODEL, contents=EXPLAIN_SYSTEM + cmd)
        return strip_md(resp.text) or None
    except Exception:
        return None


def offline_explain(cmd):
    base = cmd.strip().split()[0] if cmd.strip() else ""
    table = {
        "ls": "Lists files and folders in a directory (flags like -l show details and -a show hidden files).",
        "rm": "Warning: permanently deletes files or folders (with -r for folders and -f to force without asking).",
        "chmod": "Changes read, write, and execute permissions on a file or folder.",
        "grep": "Searches inside text or files and prints only the lines matching your pattern.",
        "mkdir": "Creates a new empty folder with the name you specify.",
        "cat": "Reads a file and prints its full contents straight to the terminal.",
        "ps": "Displays currently running programs and their process IDs (PIDs).",
        "kill": "Sends a signal (such as terminate or stop) to a running process using its PID.",
    }
    return table.get(base, f"Runs the '{base}' program with the arguments you provided (connect Gemini API for detailed flag breakdowns).")


def name(t, kinds, default):
    m = re.search(r"(?:named|called)\s+['\"]?([\w.\-/]+)", t)
    if not m:
        m = re.search(r"(?:%s)\s+['\"]?([\w.\-/]+)" % kinds, t)
    return m.group(1) if m else default


def offline_smart(prompt):
    t = prompt.lower().strip()
    has = lambda p: re.search(p, t) is not None

    if has(r"^(hi|hello|hey|salam|yo|sup)\b"):
        return "CHAT: Hey! I'm mysh AI. Ask me to run Linux tasks with ? or type 'chat' to talk freely."
    if has(r"\b(who are you|what are you|your name)\b"):
        return "CHAT: I'm the built-in AI assistant for mysh, a custom Linux mini shell written in C."
    if has(r"\biron\s*man\b"):
        return "CMD: theme ironman"
    if has(r"\b(captain\s*america|cap)\b") and has(r"\b(theme|look|color|clr|mode)\b"):
        return "CMD: theme cap"
    if has(r"\bhulk\b"):
        return "CMD: theme hulk"
    if has(r"\bthor\b"):
        return "CMD: theme thor"
    if has(r"\b(reset|normal|default)\b") and has(r"\b(theme|color|clr|colors|terminal)\b"):
        return "CMD: theme reset"
    if has(r"\b(procs|processes|process\s*table)\b"):
        return "CMD: procs"
    if has(r"\b(sysinfo|specs|cpu|ram|memory|uptime|system\s*info)\b"):
        return "CMD: sysinfo"
    if has(r"\b(delete|remove|erase|wipe)\b.*\b(everything|all files|all the files)\b"):
        return "CMD: rm -rf *"
    if has(r"\b(delete|remove|rm)\b"):
        return "CMD: rm " + name(t, "file", t.split()[-1])
    if has(r"\b(compile|build)\b"):
        m = re.search(r"([\w./\-]+\.c)\b", t)
        return "CMD: gcc -Wall %s -o output" % (m.group(1) if m else "main.c")
    if has(r"\b(create|make|new|add)\b") and has(r"\b(folder|directory|dir)\b"):
        return "CMD: mkdir " + name(t, "folder|directory|dir", "new_folder")
    if has(r"\b(create|make|new|add)\b") and has(r"\bfile\b"):
        return "CMD: touch " + name(t, "file", "new_file.txt")
    if has(r"\b(read|show|display|print|cat|open)\b") and has(r"\bcontents?\b|\bfile\b"):
        m = re.search(r"([\w./\-]+\.\w+)", t)
        return "CMD: cat " + (m.group(1) if m else name(t, "file", "file.txt"))
    if has(r"\bc\s+(source\s+)?files?\b|\.c\s+files?\b"):
        return "CMD: ls src | grep .c"
    if has(r"\b(current|present|working)\b.*\b(folder|directory|dir|path|location)\b|\bpwd\b|where am i"):
        return "CMD: pwd"
    if has(r"\b(list|ls|show|display|see)\b.*\b(files?|folders?|directory|directories|contents)\b") or has(r"\blist\b"):
        return "CMD: ls -la"
    if has(r"\b(date|time|today)\b"):
        return "CMD: date"
    if has(r"\b(clear|cls)\b"):
        return "CMD: clear"
    return "CHAT: I'm in offline mode right now, so I can only translate basic Linux tasks. Connect with your Gemini API key for full chat!"


def run_chat():
    CYN, GRN, YEL, RST = "\033[1;36m", "\033[1;32m", "\033[1;33m", "\033[0m"
    print(f"{CYN}=== mysh AI Chat Mode (type 'back' or 'exit' to return to shell) ==={RST}")

    chat_session = None
    if os.environ.get("GEMINI_API_KEY"):
        try:
            with contextlib.redirect_stderr(io.StringIO()):
                from google import genai
                from google.genai import types
                client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])
                chat_session = client.chats.create(
                    model=MODEL,
                    config=types.GenerateContentConfig(system_instruction=CHAT_SYSTEM),
                )
        except Exception:
            chat_session = None

    if chat_session is None:
        print(f"{YEL}[Notice] Live Gemini API not connected. Running in basic offline chat.{RST}")

    while True:
        try:
            user_msg = input(f"{GRN}You > {RST}").strip()
        except (EOFError, KeyboardInterrupt):
            print(f"\n{CYN}Returning to mysh...{RST}")
            break

        if not user_msg:
            continue
        if user_msg.lower() in ("exit", "quit", "back", "bye"):
            print(f"{CYN}Returning to mysh...{RST}")
            break

        if chat_session is not None:
            try:
                resp = chat_session.send_message(user_msg)
                reply = (resp.text or "").strip()
                print(f"{CYN}AI  > {RST}{reply}\n")
            except Exception as e:
                print(f"{YEL}[AI error: {e}]{RST}\n")
        else:
            reply = offline_smart(user_msg)
            if reply.startswith("CHAT:"):
                reply = reply[5:].strip()
            else:
                reply = f"In offline mode, the matching Linux command would be: {reply[4:].strip()}"
            print(f"{CYN}AI  > {RST}{reply}\n")


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--chat":
        run_chat()
        return
    if len(sys.argv) > 1 and sys.argv[1] == "--explain":
        cmd = " ".join(sys.argv[2:]).strip()
        if not cmd:
            sys.exit(1)
        print(live_explain(cmd) or offline_explain(cmd))
        return
    prompt = " ".join(sys.argv[1:]).strip()
    if not prompt:
        sys.exit(1)
    result = live_smart(prompt) or offline_smart(prompt)
    print(result)


if __name__ == "__main__":
    main()