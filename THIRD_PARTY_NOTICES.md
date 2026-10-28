# Third-party components

FoldBOcean uses third-party libraries. Their licenses apply independently of the
FoldBOcean application source.

- **JSON for Modern C++ (nlohmann/json)** — copyright © 2013–2025 Niels Lohmann,
  MIT License. The complete license is preserved in
  [native/vendor/LICENSE.json](native/vendor/LICENSE.json). Binary distributions
  include the same notice as `licenses/nlohmann-json.txt`.
- **Tauri 2 and the Rust dependencies** — versions are pinned in
  `src-tauri/Cargo.lock`; upstream components retain their respective licenses.
- **Vite and JavaScript dependencies** — versions and package sources are pinned
  in `package-lock.json`; development tooling is not included in the installer.

Segoe UI fonts, Win32, GDI+ and WebView2 are Microsoft system components and are
not licensed as FoldBOcean source. Font files are not bundled with the program.
The installer can obtain WebView2 Runtime from Microsoft when it is missing.
