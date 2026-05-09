# Resources

This folder contains first-party non-code assets that are used by build,
packaging, or runtime targets.

Keep reusable source code under `src/`. Put platform packaging resources and
editable product assets here when they are not owned by a specific runtime
module.

## Layout

- `launcher/windows/`: Windows launcher resource compiler inputs.
- `launcher/source/`: editable launcher source assets used to produce platform
  resources.
