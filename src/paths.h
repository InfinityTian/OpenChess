#ifndef PATHS_H
#define PATHS_H

/*
 * Runtime location of the assets and configuration, resolved relative to the
 * running executable so installed builds ($PREFIX/bin/openchess +
 * $PREFIX/share/openchess/assets) and macOS .app bundles work from any cwd.
 */

/* Directory containing the running executable (no trailing slash). */
const char *path_exe_dir(void);

/* assets/ directory; honours $OPENCHESS_ASSETS, then install/bundle layouts. */
const char *path_assets(void);

/* Configuration file; honours $OPENCHESS_CONFIG, ./chess.conf, then XDG. */
const char *path_config(void);

/* Create the parent directory of `file` (best effort). */
void path_make_parent(const char *file);

#endif
