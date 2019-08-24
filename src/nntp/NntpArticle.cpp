//========================================================================
//
// Copyright (C) 2019 Matthieu Bruel <Matthieu.Bruel@gmail.com>
//
// This file is a part of ngPost : https://github.com/mbruel/ngPost
//
// ngPost is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; version 3.0 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301,
// USA.
//
//========================================================================

#include "NntpArticle.h"
#include "NntpConnection.h"
#include "nntp/NntpFile.h"
#include "nntp/Nntp.h"
#include "utils/Yenc.h"
#include <sstream>
NntpArticle::NntpArticle(NntpFile *file, int part, const char data[], qint64 pos, qint64 bytes,
                         const std::string &from, const std::string &groups, bool obfuscation) :
    _nntpFile(file), _part(part),
    _id(QUuid::createUuid()),
    _from(from), _groups(groups),
    _subject(obfuscation ? "": QString("%1 (%2/%3)").arg(_nntpFile->name()).arg(part).arg(_nntpFile->nbArticles()).toStdString()),
    _filePos(pos), _fileBytes(bytes),
    _yencBody(new uchar[bytes*2]),
    _yencSize(0),
    _crc32(0xFFFFFFFF),
    _body()
{
    file->addArticle(this);
    connect(this, &NntpArticle::posted, _nntpFile, &NntpFile::onArticlePosted, Qt::QueuedConnection);

    _yencSize = Yenc::encode(data, bytes, _yencBody, _crc32);

    std::stringstream ss;
    ss << "=ybegin part=" << _part << " total=" << _nntpFile->nbArticles() << " line=128"
       << " size=" << _nntpFile->fileSize() << " name=" << _nntpFile->fileName() << Nntp::ENDLINE
       << "=ypart begin=" << _filePos + 1 << " end=" << _filePos + _fileBytes << Nntp::ENDLINE
       << _yencBody << Nntp::ENDLINE
       << "=yend size=" << _fileBytes << " pcrc32=" << std::hex << _crc32 << Nntp::ENDLINE
       << "." << Nntp::ENDLINE;
    _body = ss.str();

    delete [] _yencBody;
    _yencBody = nullptr;
}

NntpArticle::NntpArticle(const std::string &from, const std::string &groups, const std::string &subject, const std::string &body):
    _nntpFile(nullptr), _part(1),
    _id(QUuid::createUuid()),
    _from(from), _groups(groups),
    _subject(subject),
    _filePos(0), _fileBytes(0),
    _yencBody(nullptr),
    _yencSize(0),
    _crc32(0xFFFFFFFF),
    _body(body)
{
    _body += Nntp::ENDLINE;
    _body += ".";
    _body += Nntp::ENDLINE;
}

void NntpArticle::write(NntpConnection *con, const std::string &idSignature)
{
    con->write(header(idSignature).c_str());
    con->write(_body.c_str());
}

std::string NntpArticle::header(const std::string &idSignature) const
{
    std::stringstream ss;
    ss << "From: "        << _from    << Nntp::ENDLINE
       << "Newsgroups: "  << _groups  << Nntp::ENDLINE
       << "Subject: "     << (_subject.empty()?_id.toByteArray().constData():_subject) << Nntp::ENDLINE
       << "Message-ID: <" << _id.toByteArray().constData() << "@" << idSignature << ">" << Nntp::ENDLINE
       << Nntp::ENDLINE;
    return ss.str();
}

void NntpArticle::dumpToFile(const QString &path)
{
    QString fileName = QString("%1/%2_%3.yenc").arg(path).arg(_nntpFile->fileName().c_str()).arg(_part);
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "[NntpArticle::dumpToFile] error creating file " << fileName;
        return;
    }

    file.write(header("ngPost").c_str());
    file.write(_body.c_str());
    file.close();
}