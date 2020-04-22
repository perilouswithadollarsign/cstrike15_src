#ifndef SCHEMA_VERSION_NUMBERS_H_
#define SCHEMA_VERSION_NUMBERS_H_
#pragma once

/// !!NOTE!! - This file is included in the game AND schemacompiler AND vpc
/// This is where version numbers are specified to keep all of the files + binding macros in sync

/// If you need to make an incompatible change to the schema bindings, increment this
#define SCHEMA_BINDING_VERSION 32

/// If you need to change the content of schproj files, increment this
#define SCHEMA_SCHPROJ_VERSION 11

#endif
