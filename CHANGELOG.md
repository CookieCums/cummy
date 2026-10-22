# Changelog


## [1.0.3] - 2026-10-23

### Fixed

- Shell-code generation now quotes paths safely, so projects containing spaces or single quotes no longer produce broken `export` statements.
- Activation is idempotent: running `cum` when the target venv is already active no longer duplicates `PATH` entries.
- `uncum` now restores the original `PATH` exactly (`_CUMMY_SAVED_PATH`) and falls back to a safe `PATH` filter when the venv was activated outside of Cummy.
- Non-interactive stdin (CI, piped shells) no longer hangs on prompts — the saved preference is used instead, defaulting to "do not activate".
- Windows virtual environments are detected correctly (`Scripts\python.exe`) instead of only looking for `bin/python`.
- Scanner now detects `lazy import` and `lazy from ... import` statements introduced in Python 3.15 (PEP 810). Previously, dependencies imported lazily were silently omitted from `requirements.txt` because the existing regex patterns required the line to start with `import` or `from`.

### Added

- **`cummy cum`** — detects the nearest virtual environment (searches the current directory and up to 3 parent directories) and activates it in the current shell via emitted shell code.
- **`cummy uncum`** — idempotent deactivation; safe to run even when no venv is active.
- **`cum` / `uncum` shell functions** — installed via **`cummy setup`**, which prints ready-to-paste snippets for Bash, Zsh, and Fish.
- **`cummy gen install`** — scans the project, writes `requirements.txt`, and installs it. Prefers **`uv`** when available, otherwise falls back to `pip`. Reuses the active venv, otherwise offers to create `.venv` (or runs with the system interpreter).
- **`--shell`** flag on `cum`/`uncum` — forces raw shell code output even when attached to a TTY (useful for `eval`, wrapping, and debugging).
- **`--yes` / `-y`** and **`--no`** flags on `cum`/`uncum` — bypass the interactive prompt, e.g. `eval "$(cummy cum --shell --yes)"`.
- **Persistent "always / never / ask" preference** stored in `~/.config/cummy/settings` (`auto_activate=…`), so the prompt can be silenced once and forever.
- **`cummy gen install` auto-venv**: if no venv exists and the user agrees, `.venv` is created with `python -m venv`; if the user declines, `uv` (when present) will still create one implicitly, and `pip` runs against the chosen interpreter.

### Changes

- **Command dispatcher rewritten** as a `CommandKind` enum with a single shared option parser, replacing the previous ad-hoc branching in `parseArguments`. Unknown subcommands now report a clear error instead of falling through to `scan`.
- **`scan` / `gen dep` output** unified — both go through `runScanOrDep`, so `--path`, positional directories, and error messages are now consistent across commands.
- **`cummy help`** rewritten to document `cum`, `uncum`, `setup`, and `gen install`, and to explain the shell-integration model (a child process cannot mutate its parent shell, so `cum` prints code that the shell function `eval`s).
- **`cummy cum` when piped** emits only shell code on stdout; all human-readable prompts and notices are routed to stderr, so `eval "$(cummy cum)"` is safe to call even without `--shell`.

## [1.0.2] - 2026-10-13

### Fixed

- Fixed the bug with the installer not showing explicit repair/reinstall, change-location, and uninstall choices. Introduced in [1.0.1]

## [1.0.1] - 2026-10-13

### Fixed

- Keep the Command Prompt open after showing installer help so the usage text can be read.
- Handle rerunning the installer with explicit repair/reinstall, change-location, and uninstall choices.

### Added

- Add Start Menu shortcuts to the GitHub repository and latest release.
- Add version-specific release notes to the publishing workflow.

## [1.0.0]

### Added

- Initial Windows release with the portable CummY CLI and guided installer.
- Scan Python projects and generate requirements files.
