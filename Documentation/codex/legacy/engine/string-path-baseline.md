# String And Path Utility Baseline

## Purpose

Phase 40 audits shared string and path helpers before moving implementation out
of legacy C files. Most of this surface lives in `public/crtlib.*`, so it is a
public utility boundary used by engine, filesystem, renderers, and utilities.

## Public Surface

The path helper cluster currently includes:

- `COM_FileBase`
- `COM_FileExtension`
- `COM_FileWithoutPath`
- `COM_ExtractFilePath`
- `COM_StripExtension`
- `COM_DefaultExtension`
- `COM_ReplaceExtension`
- `COM_PathSlashFix`
- inline `COM_FixSlashes`

Broader string helpers remain in `public/crtlib.h` or `public/crtlib.c`:

- bounded copy/concat: `Q_strncpy`, `Q_strncat`
- null-tolerant comparisons: `Q_strcmp`, `Q_strncmp`, `Q_stricmp`,
  `Q_strnicmp`
- string search and wildcard helpers: `Q_stristr`, `Q_stricmpext`,
  `Q_strnicmpext`, `matchpattern`
- parsing and formatting helpers: `COM_ParseFileSafe`, `Q_vsnprintf`,
  `Q_snprintf`

## Path Compatibility Notes

- `COM_FileBase` accepts `/` and `\` as path separators, ignores dots before
  the final separator, truncates to the provided output size, and treats
  `.nomedia` as an empty basename.
- `COM_FileExtension` returns a pointer into the input string after the final
  dot, but returns an empty string if any `/`, `\`, or `:` appears after that
  dot.
- `COM_FileWithoutPath` treats `/`, `\`, and `:` as separators and returns the
  text after the latest one.
- `COM_ExtractFilePath` removes the final path component and strips the
  separator before it. A trailing slash is treated as a final empty component,
  so `keep/the/path/dir/` returns `keep/the/path`.
- `COM_StripExtension` strips only the final extension after the last separator.
  Leading-dot filenames such as `.nomedia` are preserved.
- `COM_DefaultExtension` appends only when the final path component has no dot
  after its first character. A leading-dot name such as `.nomedia` still gets a
  default extension appended.
- `COM_ReplaceExtension` composes strip plus default extension, so `.nomedia`
  becomes `.nomedia.cfg` when replacing with `.cfg`.
- `COM_PathSlashFix` ensures a directory path ends with `/`, converting a final
  `\` to `/`.
- inline `COM_FixSlashes` converts `\` to `/` and collapses duplicate `/`.

## Phase 40 Migration

The first migrated group is the path helper cluster:

- `src/utilities/path.cpp` owns the modern implementation.
- `src/utilities/compat/crtlib_path.cpp` exports the existing C symbols.
- `public/crtlib.c` no longer owns those non-inline path implementations.
- inline `COM_FixSlashes` remains in `public/crtlib.h`; a modern mirror exists
  as `xash::utilities::NormalizeSlashes`.

String comparison, copy/concat, parsing, and formatting helpers remain in the
public C layer for later, narrower passes.
