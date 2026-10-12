<p align="center">
  <img src="assets/cummY-github.png" alt="CummY logo" width="720">
</p>

# CummY

<p align="center">
  <img src="assets/cummY-app.png" alt="CummY app icon" width="180">
</p>

CummY is a small Windows command-line tool that scans Python projects and generates dependency lists.

## Commands

```text
cummy help
cummy scan [--path PROJECT_DIR]
cummy gen dep [OUTPUT_FILE] [--path PROJECT_DIR]
```

With no `--path`, CummY scans the current directory. The default generated file is `requirements.txt` in the project directory.

## Dependency detection

CummY reads root-level `requirements*.txt` files and dependencies from `pyproject.toml`. It also scans Python files for static `import` statements, skips known standard-library modules, and maps common import names to package names. Version constraints are retained when declared in a manifest; names inferred from imports are unpinned.

## Windows installer

Download either the [guided installer](https://github.com/CookieCums/cummy/releases/download/v1.0.0/cummy-setup-1.0.0.exe) or the [portable CLI](https://github.com/CookieCums/cummy/releases/download/v1.0.0/cummy.exe) from the first release.

The installer works per-user and offers an optional checkbox to add its installation directory to the user `PATH`. It uses the CummY app icon and creates a Start Menu shortcut.

## Publishing

Run `publish.cmd` to preview the files and approximate upload size. Run `publish.cmd -Publish -DryRun` to confirm the publish switch is passed without making changes. When ready, run `publish.cmd -Publish` to create the initial commit and `v1.0.0` tag, push them to GitHub, and publish a release with both Windows executables.
