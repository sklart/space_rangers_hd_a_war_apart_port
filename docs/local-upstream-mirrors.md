# Local upstream mirrors

The working repository keeps public upstream URLs in `.gitmodules` for CI and
other contributors. Local bare mirrors are a recovery path only; their paths
are never committed.

| mirror | public upstream | pinned revision | retained ref |
| --- | --- | --- | --- |
| `D:\repos\mirrors\SpaceRangersHD_CPP.git` | `pakompom/SpaceRangersHD_CPP` | `57fa689c630193a66fdea6ca4c79a188814991cd` | `refs/archive/space-rangers-hd-port/cpp` |
| `D:\repos\mirrors\okgf.git` | `pakompom/okgf` | `c01aa7a168a6f1772541501074b7bba1b96550ef` | `refs/archive/space-rangers-hd-port/okgf` |
| `D:\repos\mirrors\SpaceRangersHD_decomp.git` | `pakompom/SpaceRangersHD_decomp` | `730bdf6` | `refs/archive/space-rangers-hd-port/decomp-release` |

To refresh an available mirror, use `git -C <mirror> fetch --prune origin`.
Do not delete `refs/archive/space-rangers-hd-port/*`; those refs retain the
revisions needed by this port even after an upstream rewrite.

`tools/bootstrap-upstreams.ps1` uses GitHub when it is available and otherwise
writes `file:///...` URLs only to the current repository's `.git/config`.
It invokes `protocol.file.allow=always` only for its submodule command.

```powershell
pwsh -File .\tools\bootstrap-upstreams.ps1
pwsh -File .\tools\bootstrap-upstreams.ps1 -ForceLocal
```

To restore public URLs in the local checkout, run the helper without
`-ForceLocal` while GitHub is available, or unset the two local config keys and
run `git submodule sync --recursive`. A forced-local smoke clone must configure
these same two local keys before `git -c protocol.file.allow=always submodule
update --init --recursive`; it must not edit `.gitmodules`.
