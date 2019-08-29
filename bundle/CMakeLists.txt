#fixup bundle for embedding non-sys dependencies like llvm or boost

MACRO(LIB_DIRECTORIES return_list)
    FILE(GLOB_RECURSE new_list *.so)
    SET(dir_list "")
    FOREACH(file_path ${new_list})
        GET_FILENAME_COMPONENT(dir_path ${file_path} PATH)
        SET(dir_list ${dir_list} ${dir_path})
    ENDFOREACH()
    LIST(REMOVE_DUPLICATES dir_list)
    SET(${return_list} ${dir_list})
ENDMACRO()
LIB_DIRECTORIES(ADDITIONAL_LIB_PATHS)
list(APPEND ADDITIONAL_LIB_PATHS /usr/local/lib/)

install(
    CODE
    "
        include(InstallRequiredSystemLibraries)
        include(BundleUtilities)
		message(\"FIXUP BUNDLE at \${CMAKE_INSTALL_PREFIX}/bin/phasar\")
        fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/bin/phasar\"  \"\" \"${ADDITIONAL_LIB_PATHS}\")
    "
	DESTINATION lib
    COMPONENT Runtime)
