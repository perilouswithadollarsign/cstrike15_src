#ifdef SERIALIZATIONSTART
__SERIALIZATIONSTART = DIAG_START_SERIALIZATION,
#undef SERIALIZATIONSTART
#endif

DIAG(err_fe_not_a_pch_file, CLASS_ERROR, diag::MAP_ERROR, "input is not a PCH file: '%0'", 0, true, false, false, false, 0)
DIAG(err_fe_pch_file_modified, CLASS_ERROR, diag::MAP_FATAL, "file '%0' has been modified since the precompiled header '%1' was built", 0, true, false, false, false, 0)
DIAG(err_fe_pch_file_overridden, CLASS_ERROR, diag::MAP_ERROR, "file '%0' from the precompiled header has been overridden", 0, true, false, false, false, 0)
DIAG(err_fe_pch_malformed, CLASS_ERROR, diag::MAP_FATAL, "malformed or corrupted PCH file: '%0'", 0, true, false, false, false, 0)
DIAG(err_fe_pch_malformed_block, CLASS_ERROR, diag::MAP_FATAL, "malformed block record in PCH file: '%0'", 0, true, false, false, false, 0)
DIAG(err_fe_unable_to_read_pch_file, CLASS_ERROR, diag::MAP_ERROR, "unable to read PCH file %0: '%1'", 0, true, false, false, false, 0)
DIAG(err_not_a_pch_file, CLASS_ERROR, diag::MAP_FATAL, "'%0' does not appear to be a precompiled header file", 0, true, false, false, false, 0)
DIAG(err_pch_langopt_mismatch, CLASS_ERROR, diag::MAP_ERROR, "%0 was %select{disabled|enabled}1 in PCH file but is currently %select{disabled|enabled}2", 0, true, false, false, false, 0)
DIAG(err_pch_langopt_value_mismatch, CLASS_ERROR, diag::MAP_ERROR, "%0 differs in PCH file vs. current file", 0, true, false, false, false, 0)
DIAG(err_pch_macro_def_conflict, CLASS_ERROR, diag::MAP_ERROR, "definition of macro '%0' differs between the precompiled header ('%1') and the command line ('%2')", 0, true, false, false, false, 0)
DIAG(err_pch_macro_def_undef, CLASS_ERROR, diag::MAP_ERROR, "macro '%0' was %select{defined|undef'd}1 in the precompiled header but %select{undef'd|defined}1 on the command line", 0, true, false, false, false, 0)
DIAG(err_pch_targetopt_feature_mismatch, CLASS_ERROR, diag::MAP_ERROR, "%select{AST file|current translation unit}0 was compiled with the target feature'%1' but the %select{current translation unit is|AST file was}0 not", 0, true, false, false, false, 0)
DIAG(err_pch_targetopt_mismatch, CLASS_ERROR, diag::MAP_ERROR, "PCH file was compiled for the %0 '%1' but the current translation unit is being compiled for target '%2'", 0, true, false, false, false, 0)
DIAG(err_pch_undef, CLASS_ERROR, diag::MAP_ERROR, "%select{command line contains|precompiled header was built with}0 '-undef' but %select{precompiled header was not built with it|it is not present on the command line}0", 0, true, false, false, false, 0)
DIAG(err_pch_with_compiler_errors, CLASS_ERROR, diag::MAP_ERROR, "PCH file contains compiler errors", 0, true, false, false, false, 0)
DIAG(warn_module_conflict, CLASS_WARNING, diag::MAP_WARNING, "module '%0' conflicts with already-imported module '%1': %2", 215, true, false, false, false, 0)
DIAG(warn_pch_different_branch, CLASS_ERROR, diag::MAP_ERROR, "PCH file built from a different branch (%0) than the compiler (%1)", 0, true, false, false, false, 0)
DIAG(warn_pch_version_too_new, CLASS_ERROR, diag::MAP_ERROR, "PCH file uses a newer PCH format that cannot be read", 0, true, false, false, false, 0)
DIAG(warn_pch_version_too_old, CLASS_ERROR, diag::MAP_ERROR, "PCH file uses an older PCH format that is no longer supported", 0, true, false, false, false, 0)
