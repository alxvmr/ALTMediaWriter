
#include "image_type.h"

QList<ImageType *> ImageType::all() {
    static const QList<ImageType *> m_all =
    []() {
        QList<ImageType *> out;

        for (int i = 0; i < COUNT; i++) {
            const ImageType::Id id = (ImageType::Id) i;
            out.append(new ImageType(id));
        }

        return out;
    }();

    return m_all;
}

ImageType *ImageType::fromFilename(const QString &filename) {
    for (int i = 0; i < COUNT; i++) {
        ImageType *type = all()[i];

        const QStringList abbreviations = type->abbreviation();
        for (const QString abbreviation : abbreviations) {
            if (filename.endsWith(abbreviation, Qt::CaseInsensitive)) {
                return type;
            }
        }
    }
    return all()[UNKNOWN];
}

bool ImageType::isValid() const {
    return m_id != UNKNOWN;
}

QStringList ImageType::abbreviation() const {
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

QString ImageType::name() const {
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

bool ImageType::supportedForWriting() const {
    static const QList<ImageType::Id> unsupported = {
        TAR_GZ, TAR_XZ, IMG_GZ, RECOVERY_TAR, UNKNOWN, COUNT
    };

    return !unsupported.contains(m_id);
}

bool ImageType::canWriteWithRootfs() const {
#if defined(_WIN32)
    return false;
#else
    if (m_id == TAR_XZ) {
        return true;
    } else {
        return false;
    }
#endif
}

ImageType::ImageType(const ImageType::Id id_arg)
: m_id(id_arg) {

}
