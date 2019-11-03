isEmpty(target.path) {
    target.path = $${PREFIX}/usr/libexec/$$MEDIAWRITER_NAME
    export(target.path)
}
INSTALLS += target

HELPER_PATH = $${PREFIX}/usr/libexec/$$MEDIAWRITER_NAME/helper
export(HELPER_PATH)

export(INSTALLS)
