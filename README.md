# Retail Grading

This repository is reserved for the WXL module `wxl-retail-grading`. The module currently lives in the WXL v1.1 integration checkout; this documentation PR contains **no source or release DLL**. Grading and shared-renderer edits are in progress, so source should be copied only after their API and lifecycle are pinned.

## Integration and release checks

The integrated module applies a color-grade pass to the world at the world-to-UI boundary, leaving UI colors alone, and exposes tuning controls. It needs matching WXL render/event APIs, D3D9 support, environment support, shared ImGui, and the intended grading cube data in the client. A DLL alone does not supply the cubes. Document exact client data paths, ownership, and hashes separately before packaging.

Once the renderer work is committed, add a pinned source snapshot and build Win32 against the matching core. Verify zone/time changes, enabled/disabled settings, UI color isolation, device reset, teleports, performance, and rollback without reducing HD output. Keep the PR draft until the exact package and runtime route pass. Roll back by closing the client and restoring the prior compatible DLL, core, and data set.

## Credits and license

Preserve WarcraftXL source notices and the GPL-3.0 license when source is added. Furioz's local integration changes remain attributed in the integration Git history. Game textures and grading assets remain with their respective owners and are not bundled here.
