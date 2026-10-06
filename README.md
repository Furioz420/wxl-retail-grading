# Retail Grading

`wxl-retail-grading` is a WarcraftXL client extension that applies a color-grade pass to the world at the world-to-UI boundary, leaving UI colors alone. It exposes tuning controls and uses the core render/event APIs, D3D9, shared ImGui, and environment support.

## Build and installation

Place this repository under the matching `wxl-core/extensions/wxl-retail-grading` path, configure the core for Win32, and build the `wxl-retail-grading` target. The source built in Release/Win32 against Furioz420/wxl-core `853217d7b0441e95eed2ba092f291dbd6626e6c9` on 2026-10-06. This is a source compatibility check, not a packaged DLL release or gameplay acceptance. The intended grading cube data must also be installed in the client; a DLL alone does not supply it.

Before publishing a binary, verify zone/time changes, enabled/disabled settings, UI color isolation, device reset, teleports, performance, and rollback without reducing HD output. Roll back by closing the client and restoring the prior compatible DLL, core, and data set.

## Credits and license

Preserve WarcraftXL source notices and the GPL-3.0 license. Furioz's local integration changes are attributed in the Git history. Game textures and grading assets remain with their respective owners and are not bundled here.
