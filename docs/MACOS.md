# macOS (roadmap R6)

One universal program for Apple Silicon and Intel Macs, macOS 11 or later, as an app bundle.
CI builds it and tests it on both processors (the `macos` job); there is no Mac download yet
(the disk image is R6.3).

## The app bundle (R6.2)

`tools/ci/mac_bundle.sh` makes `NBA JAM Recomp.app`:

| Part | What |
|---|---|
| `Contents/MacOS/NBAJamRecomp` | the program, arm64 and x86_64 in one file (`lipo`) |
| `Contents/Info.plist` | `dist/macos/Info.plist`: ID `io.github.richvam.NBA_JAMs_Recomp`, macOS 11, Retina, games category, controller support |
| `Contents/Resources/NBAJamRecomp.icns` | the icon, every size made from `dist/linux/nbajamrecomp.svg` |
| `Contents/Resources/mods`, `overlays`, `roms`, `README.txt` | what the other packages ship next to the program |

It is signed ad hoc (`codesign -s -`), which Apple Silicon needs to run it; it is not signed
with a developer certificate (R3.3), so the first start shows the unidentified-developer warning.

## Checklist

Checked in CI on `macos-15` (Apple Silicon) and `macos-15-intel`, unless marked otherwise.

| Convention | State | How it was checked |
|---|---|---|
| App bundle with its name and icon in the Dock and menu bar | Done | screenshot of the running app (CI artifact `macos-<arch>-test-output`, `shots/desktop-*.png`) |
| Data in `~/Library/Application Support/NBAJamRecomp` | Done (SDL's per-user folder; no change was needed) | the started app made it, copied the 41 shipped scripts there, and wrote its screenshots there |
| Command keys: Cmd+Q quits (SDL's app menu), Ctrl+Cmd+F and Cmd+Return go fullscreen, Cmd+V pastes into a text field | Done | code; not pressed in CI (no keyboard events from a script) |
| The keys screen names the Mac keys (no Print Screen; F keys may need fn) | Done | code |
| Fullscreen in its own Space, with the window's green button too; the setting follows | Done (SDL's default on macOS; the setting now follows the window) | audit A switches fullscreen on and off from Settings; the green button is not clicked in CI |
| Retina | Done (the picture is sized in drawable pixels, W1.4; `NSHighResolutionCapable`) | no Retina screen in CI |
| PlayStation and Xbox controllers, wired or over Bluetooth | Supported by SDL 2.30.9 (Game Controller framework and HIDAPI) | **not checked**: no controller in CI and no Mac here |

## Online (R6.4)

- **Local Network:** macOS 15 and later ask once whether the app may use the local network.
  Allow it: the home-network search (a broadcast) and the router setup (UPnP, PCP, NAT-PMP)
  need it. The question gives the reason from `NSLocalNetworkUsageDescription` in
  `Info.plist`. If it was refused: System Settings, Privacy & Security, Local Network, turn
  NBA JAM Recomp on.
- **Firewall:** if the macOS firewall is on, the first hosted game may ask whether NBA JAM
  Recomp may accept incoming connections. Choose Allow. The firewall is off by default.
- **Router:** NAT-PMP and PCP are sent to the default gateway, which on a Mac is read from the
  routing table with `sysctl` (Linux reads `/proc/net/route`, Windows `GetIpForwardTable`).
  The `macos` CI job runs `nattest` (loopback netplay, STUN, punching, and UPnP, PCP and
  NAT-PMP against fake routers) and checks that the gateway is found.
- **Same results as Windows and Linux:** the Mac build gives the same Lua hash (luadet) as the
  other builds. A game between a Mac and another computer has **not** been played: CI runners
  cannot reach each other and have no ROMs.

## Not checked yet

A real Mac: Retina, a controller over Bluetooth, Cmd keys pressed by hand, the green button,
and a game with ROMs. The first owner of a Mac to try it can tick these off.
