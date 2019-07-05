//
// SVisual Project
// Copyright (C) 2018 by Contributors <https://github.com/Tyill/SVisual>
//
// This code is licensed under the MIT License.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "stdafx.h"
#include "webServer.h"

void webServer::incomingConnection(qintptr handle){

    clientSocket* socket = new clientSocket(this);
    socket->setSocketDescriptor(handle);
        
    connect(socket, SIGNAL(readyRead()), socket, SLOT(readData()));
    connect(socket, SIGNAL(disconnected()), socket, SLOT(deleteLater()));
}

clientSocket::clientSocket(QObject* parent) 
    : QTcpSocket(parent){
    
    http_parser_init(&parser_, HTTP_REQUEST);

    parser_.data = this;
}

void clientSocket::readData(){
       
    http_parser_settings parser_settings{0};
    
    parser_settings.on_url = [](http_parser* parser, const char* url, size_t length){
    
        struct http_parser_url u{ 0 };
        if (http_parser_parse_url(url, length, 0, &u) != 0)
            return -1;
        
        if (u.field_set & (1 << UF_PATH)) {
            for (int i = 0; i < UF_MAX; ++i){

                QString fld = QString(url).mid(u.field_data[i].off, u.field_data[i].len);

                if (!fld.isEmpty() && (fld[0] == '/'))
                    ((clientSocket*)parser->data)->reqPage_ = fld;
            }
        }
        return 0;
    };
    parser_settings.on_header_field = [](http_parser* parser, const char* url, size_t length){

        QString fld = QString::fromLocal8Bit(url, length);
        
        ((clientSocket*)parser->data)->cField_ = fld;
        ((clientSocket*)parser->data)->reqFields_[fld] = "";
        
        return 0;
    };
    parser_settings.on_header_value = [](http_parser* parser, const char* url, size_t length){

        QString fld = ((clientSocket*)parser->data)->cField_;

        ((clientSocket*)parser->data)->reqFields_[fld] = QString::fromLocal8Bit(url, length);

        return 0;
    };
    parser_settings.on_message_complete = response;

    auto buff = this->readAll();

    size_t nread = buff.size(),
           parsed = http_parser_execute(&parser_, &parser_settings, buff.data(), nread);

    if (parsed < nread) {

        bool ok = false;
    }
}

int response(http_parser* parser){
      
    QString& page = ((clientSocket*)parser->data)->reqPage_;
    auto& fields = ((clientSocket*)parser->data)->reqFields_;

    if (!fields.contains("Accept") || fields["Accept"].contains("text/html"))
        fields["Accept"] = "text/html";
    else if (fields["Accept"].contains("text/css"))
        fields["Accept"] = "text/css";

    if (page == "/")
        page = "/index.html";

    QFile file(QApplication::applicationDirPath() + "/web" + page);

    QByteArray html;
    if (file.exists()){
        
        file.open(QIODevice::ReadOnly);
              
        html = file.readAll();

        file.close();

        if (fields["Accept"] == "text/html"){
            QTextCodec* codec = QTextCodec::codecForName("utf8");
            html = qPrintable(codec->toUnicode(html));
        }
    }  
        
    QByteArray resp;
    resp += QString("HTTP/1.1 200 OK\r\n")
         + "Content-Type: " + fields["Accept"] + "\r\n"
         + "Connection: keep-alive\r\n"
         + "Content-Length: " + QString::number(html.size()) + "\r\n"
         + "\r\n";
    
    resp += html;

    ((clientSocket*)parser->data)->writeData(resp.data(), resp.size());

    return 0;
}