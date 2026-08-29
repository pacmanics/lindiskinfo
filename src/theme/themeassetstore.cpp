// SPDX-FileCopyrightText: 2026 PacmanicS
// SPDX-License-Identifier: GPL-3.0-or-later

#include "themeassetstore.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QtEndian>

#include <array>
#include <limits>

namespace
{

constexpr qsizetype AssetHeaderSize = 16;
constexpr qsizetype AssetEntrySize = 28;

constexpr quint32 AssetVersion = 1;
constexpr quint32 MaximumAssetCount = 64;


QByteArray assetKey()
{
    static constexpr std::array<quint8, 32> key =
    {
        77, 58, 201, 231,
        22, 178, 128, 95,
        138, 49, 210, 196,
        127, 233, 11, 102,
        81, 164, 143, 3,
        123, 197, 222, 41,
        232, 19, 118, 240,
        74, 201, 146, 189
    };

    return QByteArray(
        reinterpret_cast<const char *>(
            key.data()
        ),
        static_cast<qsizetype>(
            key.size()
        )
    );
}


bool readUInt32(
    const QByteArray &data,
    qsizetype offset,
    quint32 &value
)
{
    if (
        offset < 0 ||
        offset >
            data.size() - 4
    ) {
        return false;
    }

    value =
        qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar *>(
                data.constData() + offset
            )
        );

    return true;
}


bool readUInt64(
    const QByteArray &data,
    qsizetype offset,
    quint64 &value
)
{
    if (
        offset < 0 ||
        offset >
            data.size() - 8
    ) {
        return false;
    }

    value =
        qFromLittleEndian<quint64>(
            reinterpret_cast<const uchar *>(
                data.constData() + offset
            )
        );

    return true;
}


void appendUInt32(
    QByteArray &data,
    quint32 value
)
{
    for (int index = 0;
         index < 4;
         ++index) {

        data.append(
            static_cast<char>(
                (value >> (index * 8))
                & 0xffu
            )
        );
    }
}


void appendUInt64(
    QByteArray &data,
    quint64 value
)
{
    for (int index = 0;
         index < 8;
         ++index) {

        data.append(
            static_cast<char>(
                (value >> (index * 8))
                & 0xffu
            )
        );
    }
}


quint64 initialState(
    quint32 assetId,
    quint64 nonce
)
{
    QByteArray material =
        assetKey();

    appendUInt32(
        material,
        assetId
    );

    appendUInt64(
        material,
        nonce
    );

    const QByteArray digest =
        QCryptographicHash::hash(
            material,
            QCryptographicHash::Sha256
        );

    if (digest.size() < 8)
        return 0x9e3779b97f4a7c15ULL;

    quint64 state =
        qFromLittleEndian<quint64>(
            reinterpret_cast<const uchar *>(
                digest.constData()
            )
        );

    if (state == 0)
        state =
            0x9e3779b97f4a7c15ULL;

    return state;
}


quint64 nextWord(
    quint64 &state
)
{
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;

    return state *
        0x2545f4914f6cdd1dULL;
}


void transform(
    QByteArray &data,
    quint32 assetId,
    quint64 nonce
)
{
    quint64 state =
        initialState(
            assetId,
            nonce
        );

    qsizetype offset = 0;

    while (offset < data.size()) {
        const quint64 word =
            nextWord(state);

        for (int byteIndex = 0;
             byteIndex < 8 &&
             offset < data.size();
             ++byteIndex,
             ++offset) {

            const quint8 source =
                static_cast<quint8>(
                    data.at(offset)
                );

            const quint8 mask =
                static_cast<quint8>(
                    (
                        word >>
                        (byteIndex * 8)
                    )
                    & 0xffu
                );

            data[offset] =
                static_cast<char>(
                    source ^ mask
                );
        }
    }
}


QByteArray assetPackage()
{
    QFile file(
        QStringLiteral(
            ":/assets/lindiskinfo-assets.bin"
        )
    );

    if (!file.open(
            QIODevice::ReadOnly
        )) {
        return {};
    }

    return file.readAll();
}

}


namespace ThemeAssetStore
{

QPixmap pixmap(
    quint32 assetId
)
{
    const QByteArray package =
        assetPackage();

    if (
        package.size() <
        AssetHeaderSize
    ) {
        return {};
    }

    if (
        package.left(8) !=
        QByteArrayLiteral("LDIAPKG1")
    ) {
        return {};
    }

    quint32 version = 0;
    quint32 count = 0;

    if (
        !readUInt32(
            package,
            8,
            version
        ) ||
        !readUInt32(
            package,
            12,
            count
        )
    ) {
        return {};
    }

    if (
        version != AssetVersion ||
        count == 0 ||
        count > MaximumAssetCount
    ) {
        return {};
    }

    const quint64 packageSize =
        static_cast<quint64>(
            package.size()
        );

    const quint64 tableEnd =
        static_cast<quint64>(
            AssetHeaderSize
        ) +
        static_cast<quint64>(
            count
        ) *
        static_cast<quint64>(
            AssetEntrySize
        );

    if (
        tableEnd >
        packageSize
    ) {
        return {};
    }

    for (quint32 index = 0;
         index < count;
         ++index) {

        const qsizetype entryOffset =
            AssetHeaderSize +
            static_cast<qsizetype>(
                index
            ) *
            AssetEntrySize;

        quint32 storedId = 0;
        quint64 dataOffset = 0;
        quint64 dataSize = 0;
        quint64 nonce = 0;

        if (
            !readUInt32(
                package,
                entryOffset,
                storedId
            ) ||
            !readUInt64(
                package,
                entryOffset + 4,
                dataOffset
            ) ||
            !readUInt64(
                package,
                entryOffset + 12,
                dataSize
            ) ||
            !readUInt64(
                package,
                entryOffset + 20,
                nonce
            )
        ) {
            return {};
        }

        if (
            storedId != assetId
        ) {
            continue;
        }

        if (
            dataOffset <
                tableEnd ||
            dataOffset >
                packageSize ||
            dataSize >
                packageSize -
                dataOffset
        ) {
            return {};
        }

        if (
            dataSize >
            static_cast<quint64>(
                std::numeric_limits<
                    qsizetype
                >::max()
            )
        ) {
            return {};
        }

        QByteArray decoded =
            package.mid(
                static_cast<qsizetype>(
                    dataOffset
                ),
                static_cast<qsizetype>(
                    dataSize
                )
            );

        transform(
            decoded,
            assetId,
            nonce
        );

        if (
            decoded.size() < 4 ||
            static_cast<quint8>(
                decoded.at(0)
            ) != 0xffu ||
            static_cast<quint8>(
                decoded.at(1)
            ) != 0xd8u ||
            static_cast<quint8>(
                decoded.at(2)
            ) != 0xffu
        ) {
            return {};
        }

        QPixmap result;

        if (
            !result.loadFromData(
                decoded,
                "JPEG"
            )
        ) {
            return {};
        }

        return result;
    }

    return {};
}

}
