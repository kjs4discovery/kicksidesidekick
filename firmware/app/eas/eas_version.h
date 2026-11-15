/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What upstream's CMake generates from host_src/eas_version.cmake: the library version of the vendored copy
 * (EmbeddedSynth/sonivox 86bd073, v4.0.x). */
#ifndef FW_EAS_VERSION_H
#define FW_EAS_VERSION_H
#define MAKE_LIB_VERSION(a, b, c, d) (((((((EAS_U32)(a) << 8) | (EAS_U32)(b)) << 8) | (EAS_U32)(c)) << 8) | (EAS_U32)(d))
#define LIB_VERSION MAKE_LIB_VERSION(4, 0, 2, 0)
#endif
