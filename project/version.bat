@ECHO OFF

REM TODO - this does not check for a version change.
REM hence will force lots of rebuilding

SET GitVersion=Unknown
SET GitBranch=Unknown
for /f "tokens=*" %%a in ('git describe --dirty') do (SET GitVersion=%%a)
for /f "tokens=*" %%a in ('git branch --no-color') do (SET GitBranch=%%a)

echo #ifndef TORC_SOURCE_VERSION>../libs/libtorc-core/version.h
echo #define TORC_SOURCE_VERSION "%GitVersion%">>../libs/libtorc-core/version.h
echo #define TORC_SOURCE_PATH    "%GitBranch%">>../libs/libtorc-core/version.h
echo #endif>>../libs/libtorc-core/version.h

REM This will need to be fixed!

echo #ifndef TORC_CORE_CONFIG_H>../libs/libtorc-core/torcconfig.h
echo #endif>>../libs/libtorc-core/torcconfig.h
