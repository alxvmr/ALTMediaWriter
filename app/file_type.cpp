
#include "file_type.h"

QList<FileType *> FileType::all() {
    static const QList<FileType *> m_all =
    []() {
        QList<FileType *> out;

        for (int i = 0; i < COUNT; i++) {
            const FileType::Id id = (FileType::Id) i;
            out.append(new FileType(id));
        }

        return out;
    }();

    return m_all;
}

FileType *FileType::fromFilename(const QString &filename) {
    FileType *matching_type = all()[UNKNOWN];
    QString matching_extension = QString();

    for (auto type : all()) {
        const QStringList extensions = type->extension();
        for (const QString extension : extensions) {
            // NOTE: need to select the longest extension for cases like ".tar" and "recovery.tar"
            const bool extension_matches = filename.endsWith(extension, Qt::CaseInsensitive);
            const bool this_extension_is_longer = (extension.length() > matching_extension.length());

            if (extension_matches && this_extension_is_longer) {
                matching_type = type;
                matching_extension = extension;
            }
        }
    }

    return matching_type;
}

bool FileType::isValid() const {
    return m_id != UNKNOWN;
}

QStringList FileType::extension() const {
    switch (m_id) {
        case ISO: return {"iso", "dvd"};
        case TAR: return {"tar"};
        case TAR_GZ: return {"tgz", "tar.gz"};
        case TAR_XZ: return {"archive", "tar.xz"};
        case IMG: return {"img"};
        case IMG_GZ: return {"igz", "img.gz"};
        case IMG_XZ: return {"ixz", "img.xz"};
        case RECOVERY_TAR: return {"trc", "recovery.tar"};
        case UNKNOWN: return {};
        case COUNT: return {};
    }
    return QStringList();
}

QString FileType::name() const {
    switch (m_id) {
        case ISO: return tr("ISO DVD");
        case TAR: return {"TAR Archive"};
        case TAR_GZ: return tr("GZIP TAR Archive");
        case TAR_XZ: return tr("LZMA TAR Archive");
        case IMG: return tr("IMG");
        case IMG_GZ: return tr("GZIP IMG");
        case IMG_XZ: return tr("LZMA IMG");
        case RECOVERY_TAR: return tr("Recovery TAR Archive");
        case UNKNOWN: return tr("Unknown");
        case COUNT: return QString();
    }
    return QString();
}

bool FileType::canWrite() const {
    static const QList<FileType::Id> supported_types = {
        ISO, IMG, IMG_XZ
    };

    return supported_types.contains(m_id);
}

FileType::FileType(const FileType::Id id_arg)
: m_id(id_arg)
{

}
