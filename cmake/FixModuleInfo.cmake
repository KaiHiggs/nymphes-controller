# cmake/FixModuleInfo.cmake — make the VST3 moduleinfo.json strict JSON
#
# JUCE's vst3_helper writes moduleinfo.json with trailing commas (JSON5). Some
# hosts parse it as strict JSON and silently skip a plug-in whose manifest does
# not parse, so the bundle we ship must carry strict JSON. Run as:
#   cmake -DMODULEINFO=<path to moduleinfo.json> -P FixModuleInfo.cmake
# Fails the build when the result is still not valid JSON.

if (NOT EXISTS "${MODULEINFO}")
    message(FATAL_ERROR "FixModuleInfo: ${MODULEINFO} does not exist")
endif()

file(READ "${MODULEINFO}" text)

# A comma followed only by whitespace before a closing } or ] is a trailing comma.
string(REGEX REPLACE ",([ \t\r\n]*[]}])" "\\1" fixed "${text}")

# Validate: string(JSON) errors on anything that is not strict JSON.
string(JSON kind ERROR_VARIABLE json_error TYPE "${fixed}")
if (json_error)
    message(FATAL_ERROR "FixModuleInfo: ${MODULEINFO} is still not valid JSON: ${json_error}")
endif()

if (NOT fixed STREQUAL text)
    file(WRITE "${MODULEINFO}" "${fixed}")
    message(STATUS "FixModuleInfo: removed trailing commas from ${MODULEINFO}")
endif()
