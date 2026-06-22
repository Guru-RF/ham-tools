# winget manifests

The [winget](https://learn.microsoft.com/windows/package-manager/) manifest for
`Guru-RF.ham-tools`, laid out exactly as it appears in the community repo:

```text
manifests/g/Guru-RF/ham-tools/<version>/
  Guru-RF.ham-tools.yaml              # version
  Guru-RF.ham-tools.installer.yaml    # zip + portable, x64 + arm64
  Guru-RF.ham-tools.locale.en-US.yaml # metadata
```

It installs the portable `.zip` archives built by
[`.github/workflows/windows.yml`](../.github/workflows/windows.yml) for both
**x64** and **arm64**, exposing `qrz`, `qte`, `dxsummit`, `dxheat` and
`holycluster` on the user's `PATH`.

The `0.2.1` manifest is filled in and validated (`winget validate` passes; the
installer SHA256s match the published release zips). The binaries are linked
statically, so the portable .zip carries no DLLs — winget runs each exe via a
symlink whose directory is not on the DLL search path, which made the earlier
dynamically-linked 0.2.0 build fail validation with STATUS_DLL_NOT_FOUND.

## Publishing a new version

1. Push a version tag (e.g. `git tag v0.3.0 && git push origin v0.3.0`). The
   Windows workflow builds both architectures and attaches
   `ham-tools-<version>-windows-x64.zip` and `…-arm64.zip` (plus `.sha256`
   files) to the GitHub Release.
2. Copy `manifests/g/Guru-RF/ham-tools/0.2.1/` to a new `<version>/` directory,
   bump `PackageVersion` and the installer URLs, and paste the real
   `InstallerSha256` for each architecture (from the release `.sha256` assets,
   or `winget hash <zip>`).
3. Validate locally (point at the version directory, not the parent):

   ```pwsh
   winget validate --manifest winget\manifests\g\Guru-RF\ham-tools\<version>
   ```

4. Submit by copying that `manifests/g/Guru-RF/ham-tools/<version>/` directory
   into a fork of
   [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) and opening
   a PR. (The [wingetcreate](https://github.com/microsoft/winget-create) tool can
   automate steps 2–4.)

Once merged, users install with:

```pwsh
winget install Guru-RF.ham-tools
```
