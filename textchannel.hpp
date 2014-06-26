/*
    Copyright (C) 2014 Alexandr Akulich <akulichalexander@gmail.com>

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
    OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef MORSE_TEXTCHANNEL_HPP
#define MORSE_TEXTCHANNEL_HPP

#include <TelepathyQt/BaseChannel>

class MorseTextChannel;

typedef Tp::SharedPtr<MorseTextChannel> MorseTextChannelPtr;

class MorseTextChannel : public Tp::BaseChannelTextType
{
    Q_OBJECT
public:
    static MorseTextChannelPtr create(QObject *connection, Tp::BaseChannel *baseChannel, uint targetHandle, const QString &identifier);
    virtual ~MorseTextChannel();

    QString sendMessageCallback(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error);

private:
    MorseTextChannel(QObject *connection, Tp::BaseChannel *baseChannel, uint targetHandle, const QString &identifier);

    QObject *m_connection;

    QString m_identifier;

    Tp::BaseChannelTextTypePtr m_channelTextType;
    Tp::BaseChannelMessagesInterfacePtr m_messagesIface;

};

#endif // MORSE_TEXTCHANNEL_HPP
