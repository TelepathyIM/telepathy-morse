/*
    This file is part of the telepathy-morse connection manager.
    Copyright (C) 2017 Alexandr Akulich <akulichalexander@gmail.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "filetransferchannel.hpp"

#include "connection.hpp"
#include "requestdetails.hpp"

#include <TelegramQt/FileOperation>
#include <TelegramQt/FilesApi>

#include <TelepathyQt/IODevice>

MorseFileTransferChannel::MorseFileTransferChannel(MorseConnection *connection, Tp::BaseChannel *baseChannel, const RequestDetails &details)
    : Tp::BaseChannelFileTransferType(details), m_localAbort(false)
{
    m_ioChannel = new Tp::IODevice();
    m_ioChannel->open(QIODevice::ReadWrite);

    Telegram::Client::FilesApi *api = connection->filesApi();
    Telegram::Client::FileOperation *operation = api->downloadFile(details.fileId(), m_ioChannel);

    connect(operation, &Telegram::Client::FileOperation::finished,
            this, &MorseFileTransferChannel::onTransferFinished);

    remoteProvideFile(m_ioChannel);
}

void MorseFileTransferChannel::onTransferFinished()
{

}

MorseFileTransferChannelPtr MorseFileTransferChannel::create(MorseConnection *connection, const RequestDetails &details, Tp::DBusError *error)
{
    if (!details.isRequested()) {
        // Morse should never ever create a Transfer channel without a request.
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("All Transfer channels must be requested"));
        return MorseFileTransferChannelPtr();
    }
    const QString fileId = details.fileId();
    if (fileId.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("No file identifier provided"));
        return MorseFileTransferChannelPtr();
    }
    MorseFileTransferChannelPtr fileTransferChannel = MorseFileTransferChannel(connection, baseChannel.data(), details);

    return fileTransferChannel;
}
