/*
    This file is part of the telepathy-morse connection manager.
    Copyright (C) 2020 Alexandr Akulich <akulichalexander@gmail.com>

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

#ifndef MORSE_REQUEST_DETAILS_HPP
#define MORSE_REQUEST_DETAILS_HPP

#include <QVariantMap>
#include <TelepathyQt/Constants>
#include <TelepathyQt/SharedPtr>

namespace Tp {

class BaseConnection;

} // Tp namespace

class RequestDetails : public QVariantMap
{
public:
    RequestDetails() = default;
    RequestDetails(const RequestDetails &details) = default;
    RequestDetails(const QVariantMap &details) :
        QVariantMap(details)
    {
    }

    RequestDetails& operator=(const QVariantMap &details)
    {
        *(static_cast<QVariantMap*>(this)) = details;
        return *this;
    }

    QString channelType() const;
    Tp::HandleType targetHandleType() const;
    uint initiatorHandle() const;
    bool isRequested() const;

    QString getTargetIdentifier(Tp::BaseConnection *connection) const;
    uint getTargetHandle(Tp::BaseConnection *connection) const;
};

#endif // MORSE_REQUEST_DETAILS_HPP
