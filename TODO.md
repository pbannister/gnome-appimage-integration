# TODO

## Pending

* [ ] make the `install` target depend on `build`: `scripts/program-install.sh` copies whatever is in `dataflow.out/build/`, so `make install` after a source change installs the previous binary and reports the previous version unless `make build` ran first.
* [ ] decide how long to keep the replaced file: `update` keeps it as `<name>.previous`, which for FreeCAD is another 820 MB. A retention rule (delete on the next successful update, or on request) would be worth having.
* [ ] use a zsync client for updates when one is installed, so an update transfers a delta instead of the whole file. `zsync`, `zsync2` and `appimageupdatetool` are all absent here, so `update` downloads in full today; the check already prints the size.
* [ ] decide what to do about the version a file name states: `version_of_appimage` reads OrcaSlicer's `…V2.4.2_62a8fff…` as `2.4`, which is what an update check would compare against a release tag.

## Open Questions

* [ ] improve the human-oriented documents in `documents/`.
* [ ] read the tool-universe sources in `documents/02-tool-universe.md`.
