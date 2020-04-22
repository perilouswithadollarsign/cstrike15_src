
//================= Copyright Valve Corporation, All rights reserved. =================//
//
//	Property Enumerations
//
//=====================================================================================//

// General
PROPERTYNAME( ANDROID_GENERAL, MinAndroidAPILevel )
PROPERTYNAME( ANDROID_GENERAL, AndroidAPILevel )
PROPERTYNAME( ANDROID_GENERAL, TargetName )
PROPERTYNAME( ANDROID_GENERAL, OutputDirectory )
PROPERTYNAME( ANDROID_GENERAL, IntermediateDirectory )
PROPERTYNAME( ANDROID_GENERAL, ConfigurationType )
PROPERTYNAME( ANDROID_GENERAL, PackageName )
PROPERTYNAME( ANDROID_GENERAL, LauncherActivity )
PROPERTYNAME( ANDROID_GENERAL, Permissions ) //Hacky, should be more like the $File syntax so we can set minsdk on individual permissions and remove them in leafy projects

// Compiler
PROPERTYNAME( ANDROID_COMPILER, PreprocessorDefinitions )

// Debugging
PROPERTYNAME( ANDROID_DEBUGGING, DebuggerFlavor )
PROPERTYNAME( ANDROID_DEBUGGING, PackagePath )
PROPERTYNAME( ANDROID_DEBUGGING, LaunchActivity )
PROPERTYNAME( ANDROID_DEBUGGING, AdditionalSymbolSearchPaths )
PROPERTYNAME( ANDROID_DEBUGGING, AdditionalSourceSearchPaths )

// Ant
PROPERTYNAME( ANDROID_ANT, AndroidAppLibName )
PROPERTYNAME( ANDROID_ANT, ApplicationName )
PROPERTYNAME( ANDROID_ANT, WorkingDirectory )
PROPERTYNAME( ANDROID_ANT, AdditionalOptions )
