# winget manifests

These three files are the [winget](https://learn.microsoft.com/windows/package-manager/)
manifest for `Guru-RF.ham-tools`. They install the portable `.zip` archives
produced by [`.github/workflows/windows.yml`](../.github/workflows/windows.yml)
for both **x64** and **arm64**, exposing `qrz`, `qte`, `dxsummit`, `dxheat`
and `holycluster` on the user's `PATH`.

## Publishing a new version

1. Push a version tag (e.g. `git tag v0.2.0 && git push --tags`). The Windows
   workflow builds both architectures and attaches
   `ham-tools-<version>-windows-x64.zip` and `…-arm64.zip` (plus `.sha256`
   files) to the GitHub Release.
2. In these manifests, bump `PackageVersion` and the URLs, and paste the real
   `InstallerSha256` for each architecture (from the `.sha256` artifacts, or
   `winget hash <zip>`).
3. Validate locally:

   ```pwsh
   winget validate --manifest winget\
   winget install --manifest winget\   # optional: test the install
   ```

4. Submit to the community repo by copying the three files to
   `manifests/g/Guru-RF/ham-tools/<version>/` in a fork of
   [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) and
   opening a PR. (The [wingetcreate](https://github.com/microsoft/winget-create)
   tool can automate steps 2–4.)

Once merged, users install with:

```pwsh
winget install Guru-RF.ham-tools
```
