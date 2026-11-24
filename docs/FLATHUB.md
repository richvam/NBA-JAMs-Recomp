# Submitting to Flathub

This is for the project's owner: a Flathub submission is made, and its listing kept up, by a
person with a GitHub account, not by CI. Everything that can be prepared is in this repository.

| File | What it is |
|---|---|
| `flatpak/io.github.richvam.NBA_JAMs_Recomp.yml` | The manifest CI builds and tests (from the checked-out folder) |
| `tools/flathub/make_manifest.py` | Turns it into the manifest Flathub wants: the source fixed to a release tag and its commit |
| `dist/linux/io.github.richvam.NBA_JAMs_Recomp.metainfo.xml` | The store page: name, summary, description, rating, releases |
| `dist/linux/nbajamrecomp.svg` | The icon (PNG sizes are made from it during the build) |

The app ID is `io.github.richvam.NBA_JAMs_Recomp`: Flathub checks that an `io.github.<user>.<name>`
ID matches a real repository, `github.com/richvam/NBA-JAMs-Recomp` (a `-` in the repository
name becomes `_` in the ID).

## Before submitting

1. **Screenshots.** Flathub requires at least one. They must be hosted at a fixed URL, and
   should show no game content (no ROM pictures). The program's own menus with no ROMs
   loaded work: add them to `dist/linux/...metainfo.xml` as
   `<screenshots><screenshot type="default"><image>URL</image></screenshot></screenshots>`.
2. **The name.** "NBA JAM" is a trademark of other companies. Flathub reviewers may ask about
   it, or for a name that does not suggest an official product. Decide before submitting.
3. **A release.** Flathub builds a fixed version. Tag one (for example `v2026.12.01`) and add a
   `<release version="2026.12.01" date="2026-12-01"/>` line at the top of `<releases>`.
4. CI's Flatpak job runs Flathub's own linter on the submission manifest (step "Flathub's
   linter"). Before submitting, it should show no errors except `appstream-missing-appinfo-file`,
   which only CI gives: CI turns off the compose step that Flathub runs itself.

## Submitting

```
python3 tools/flathub/make_manifest.py v2026.12.01
```
writes `io.github.richvam.NBA_JAMs_Recomp.yml` with the tag and its commit. Then:

1. Fork `https://github.com/flathub/flathub` and make a branch from its `new-pr` branch.
2. Add only that file at the top of the branch, commit, push.
3. Open a pull request against `new-pr`, titled `Add io.github.richvam.NBA_JAMs_Recomp`, and
   answer the checklist in the template.
4. Reviewers build it and comment; answer in the pull request. When it is accepted, Flathub
   makes a repository `flathub/io.github.richvam.NBA_JAMs_Recomp` that you maintain.

## Each new release afterwards

Run `make_manifest.py` with the new tag, add the release line to the metainfo, and open a
pull request with the new manifest in `flathub/io.github.richvam.NBA_JAMs_Recomp`.
