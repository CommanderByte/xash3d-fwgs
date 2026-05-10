# Modern Public Utility Notes

This folder tracks modernization plans for implementation code currently living
under `public/`.

The default policy is conservative:

- public headers stay as the C ABI surface;
- implementation-only code can move into `src/utilities`;
- compatibility exports live under `src/utilities/compat`;
- vendored/drop-in code stays put unless a dedicated phase says otherwise.

## Documents

- [public-folder-sweep-roadmap.md](public-folder-sweep-roadmap.md): ranked plan
  for moving remaining easy `public/` implementation islands.
