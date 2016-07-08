/*
    Copyright (C) 2016 Alexandr Akulich <akulichalexander@gmail.com>

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
    OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef MORSE_IDENTIFIER_HPP
#define MORSE_IDENTIFIER_HPP

#include <TelegramQt/TelegramNamespace>

class MorseIdentifier
{
public:
    MorseIdentifier();
    MorseIdentifier(const QString &string);

    bool operator==(const MorseIdentifier &identifier) const;
    bool operator!=(const MorseIdentifier &identifier) const;

    bool isNull() const { return !m_userId && !m_chatId; }

    quint32 userId() const { return m_userId; }
    quint32 chatId() const { return m_chatId; }
    TelegramNamespace::Peer toPeer() const;
    static MorseIdentifier fromPeer(const TelegramNamespace::Peer &peer);
    static MorseIdentifier fromChatId(quint32 chatId);
    static MorseIdentifier fromUserId(quint32 userId);
    static MorseIdentifier fromUserInChatId(quint32 chatId, quint32 userId);

    QString toString() const;
    static MorseIdentifier fromString(const QString &string);

private:
    quint32 m_userId;
    quint32 m_chatId;
};

#endif // MORSE_IDENTIFIER_HPP
