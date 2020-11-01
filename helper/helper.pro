TEMPLATE = subdirs

linux {
    SUBDIRS = linux
}
win32 {
    SUBDIRS = win
}

lupdate_only {
    SUBDIRS = linux win
}
