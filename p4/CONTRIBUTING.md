# Contributing

Thanks for helping. This is a small hobby project; keep changes focused and tested.

## Build
You need ESP-IDF **v5.5.1** and the board. Steps: [docs/dev-setup.md](docs/dev-setup.md).

```sh
export PATH=/usr/bin:$PATH; . ~/esp/esp-idf/export.sh
cp main/secrets.example.h main/secrets.h        # then edit; this file is ignored by git
idf.py set-target esp32p4 && idf.py build
```
- Put configuration in `sdkconfig.defaults`, never in the generated `sdkconfig`
  (`rm sdkconfig && idf.py set-target esp32p4` applies it).
- After adding or removing a source file under `main/`, run `idf.py reconfigure` (the file glob is read at
  configure time).
- New code should build without warnings. LVGL's deprecated `lv_list` is a known leftover.

## Test
- `test/host/run.sh`: builds and runs the Bluetooth toy encoder tests on the host with `g++`. No board needed.
  It needs `managed_components/` (run one `idf.py build` first).
- Test cloud and control code against an emulator of the Autoblow API (see [docs/testing.md](docs/testing.md)).
  **Never send commands to a real device while testing.**
- On hardware: flash, watch the serial log, and check the screen. State in your change what you could and could
  not verify on a board.

## The consistency rule
From [docs/feature-matrix.md](docs/feature-matrix.md): every user-visible feature that exists on one supported
panel should exist, with the same behaviour and look, on every hardware target. Add it to all targets in the same
piece of work, or ship it on one and open the backport straight away. A genuine technical limit is written down in
the matrix; an undocumented gap is a bug. Shared rules stay identical everywhere: Looser = stroke-minus, Tighter =
stroke-plus; Autopilot is this project's own generator (do not import patterns or numbers from Autoblow's apps);
the loss and low-battery safety behaviour; the colour roles and themes.

## Branches and commits
- Work on a topic branch off `integration` (the current line of work); do not commit to `main`/`master`
  directly, and do not force-push. Merge points get a pull request with release notes.
- Small, logical commits. Subject line in the imperative or descriptive style used in `git log` (for example
  "Colour themes: Daylight, Midnight, ... (2.0.0-p4.3)"), a body that says why. Prefix documentation-only commits
  with `docs:`.
- Bump `FW_VERSION` in `main/version.h` (`2.0.0-p4.N`) for every change that is flashed to a board; a release
  drops the suffix.
- Add a line to [CHANGELOG.md](CHANGELOG.md) under *Unreleased* for anything a user or developer would notice.
- Update the docs in the same commit as the behaviour. If you add a dependency, update
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Secrets
**Never commit secrets**: `main/secrets.h`, Wi-Fi names and passwords, Autoblow device tokens, update passwords,
real device addresses. They belong in the ignored `main/secrets.h`. Do not paste a token into an issue, a log or a
screenshot; the Settings tab shows the last three characters of the token, so never publish a screenshot of it.
Do not commit `build/`, `managed_components/`, `sdkconfig` or `dist/`.

## Code style
Match the surrounding code. Every source file starts with a short comment saying what it is for. Remove dead code
rather than commenting it out. Do not change behaviour in a clean-up commit.

## Licence
By contributing you agree that your contribution is licensed under the project's licence (CC BY-NC-SA 4.0, see
[LICENSE](LICENSE)).
