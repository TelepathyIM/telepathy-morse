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

#ifndef MORSE_FILETRANSFERCHANNEL_HPP
#define MORSE_FILETRANSFERCHANNEL_HPP

#include <TelepathyQt/BaseChannel>

namespace Tp {

class IODevice;

} // Tp

namespace Telegram {

namespace Client {

class FileOperation;

} // Client

} // Telegram

class MorseConnection;
class MorseFileTransferChannel;
class RequestDetails;

typedef Tp::SharedPtr<MorseFileTransferChannel> MorseFileTransferChannelPtr;

class MorseFileTransferChannel : public Tp::BaseChannelFileTransferType
{
    Q_OBJECT
public:
    static MorseFileTransferChannelPtr create(MorseConnection *connection, const RequestDetails &details, Tp::DBusError *error);

private:
    MorseFileTransferChannel(MorseConnection *connection, Tp::BaseChannel *baseChannel, const RequestDetails &details);

private slots:
    void onTransferFinished();

private:
    Telegram::Client::FileOperation *m_fileOperation = nullptr;
    Tp::IODevice *m_ioChannel = nullptr;
    bool m_localAbort;
};

#endif // MORSE_FILETRANSFERCHANNEL_HPP
