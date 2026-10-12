<div align="center">

<img src="assets/cummY-github-transparent.png" alt="CummY — Python dependency scanner for Windows" width="100%">

<br>

### A tiny CLI to make Python dependency discovery easier.

Scan Python projects, detect dependencies, and generate a `requirements.txt` — without the busywork.

<br>

[![Latest Release](https://img.shields.io/github/v/release/CookieCums/cummy?style=for-the-badge&color=6C9EFF&label=release)](https://github.com/CookieCums/cummy/releases/latest)
[![Platform](https://img.shields.io/badge/platform-Windows-253B70?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/CookieCums/cummy)
[![Language](https://img.shields.io/badge/language-C%2B%2B-5C8DFF?style=for-the-badge&logo=cplusplus&logoColor=white)](https://github.com/CookieCums/cummy)

<br>

[**Download Installer**](https://github.com/CookieCums/cummy/releases/download/v1.0.1/cummy-setup-1.0.1.exe)
&nbsp; · &nbsp;
[**Portable CLI**](https://github.com/CookieCums/cummy/releases/download/v1.0.1/cummy.exe)
&nbsp; · &nbsp;
[**View Releases**](https://github.com/CookieCums/cummy/releases)

</div>

---

## ✦ What is CummY?

CummY is a lightweight Windows command-line utility that scans Python projects and builds dependency lists. It combines manifest files with static import scanning to help you prepare a dependency file for a project.

<table>
  <tr>
    <td width="50%">
      <strong>⌕ &nbsp; Scan projects</strong><br>
      Find dependencies in Python source files and project manifests.
    </td>
    <td width="50%">
      <strong>▤ &nbsp; Generate requirements</strong><br>
      Create a `requirements.txt` automatically.
    </td>
  </tr>
  <tr>
    <td width="50%">
      <strong>ϟ &nbsp; Lightweight CLI</strong><br>
      Run scans from a terminal with simple commands.
    </td>
    <td width="50%">
      <strong>⬇ &nbsp; Two ways to install</strong><br>
      Choose the guided Windows installer or the portable executable.
    </td>
  </tr>
</table>

## ⚡ Quick start

### 1. Install

Download the [guided installer](https://github.com/CookieCums/cummy/releases/download/v1.0.1/cummy-setup-1.0.1.exe) or the [portable CLI](https://github.com/CookieCums/cummy/releases/download/v1.0.1/cummy.exe) from the release.

### 2. Open a terminal

Navigate to your Python project directory, or pass the directory with `--path`.

### 3. Run CummY

```powershell
cummy help
cummy scan --path "C:\path\to\project"
cummy gen dep --path "C:\path\to\project"
```

With no `--path`, CummY scans the current directory. The default output is `requirements.txt` in the project directory.

## ⌘ Commands

| Command | What it does |
| --- | --- |
| `cummy help` | Show available commands and usage. |
| `cummy scan [--path PROJECT_DIR]` | Scan a project for dependencies. |
| `cummy gen dep [OUTPUT_FILE] [--path PROJECT_DIR]` | Generate a dependency list. |

## 🔎 How dependency detection works

CummY checks:

- Root-level `requirements*.txt` files.
- Dependencies declared in `pyproject.toml`.
- Static `import` statements in Python files.
- Known standard-library modules, which are excluded from package results.
- Common import-name-to-package-name mappings.

Version constraints are retained when declared in a manifest. Dependencies inferred only from imports are listed without pinned versions.

## 🪟 Windows installation

The per-user installer uses the CummY app icon, creates a Start Menu shortcut, and optionally adds the installation directory to your user `PATH`. The portable executable can be run without the guided installer.

The installer also adds shortcuts to the CummY GitHub repository and latest release. Its **Show Cummy help** option opens a Command Prompt that stays open so you can read the commands; type `exit` when you are done.

If CummY is already installed, rerunning the installer lets you:

- **Repair / reinstall** CummY in its current location.
- **Change the installation location** by selecting a new folder; the old installation is removed when you confirm installation.
- **Uninstall** CummY and exit setup.

<div align="center">

[**Get CummY v1.0.1 →**](https://github.com/CookieCums/cummy/releases/tag/v1.0.1)
&nbsp; · &nbsp;
[**GitHub repository**](https://github.com/CookieCums/cummy)

</div>

## 📦 Publishing a release

The publishing script defaults to the `v1.0.1` hotfix and reads that version's release notes from [CHANGELOG.md](CHANGELOG.md). Build `output\cummy.exe` and `output\cummy-setup-1.0.1.exe` first, then preview and publish. The publisher packages both executables and a version-specific changelog into `output\1.0.1 (latest)\`.

```powershell
publish.cmd
publish.cmd -Publish -DryRun
publish.cmd -Publish
```

Use `-Version 1.0.2` for a later version after adding its matching section to `CHANGELOG.md` and building the correspondingly named installer. After the GitHub release succeeds, the former `(latest)` folder is renamed to its plain version and the new version folder becomes `(latest)`. Build outputs under `output\` are ignored by Git.

---

<div align="center">

Made for Windows · Built with C++

</div>
