/*
 * xml2txt.cpp
 *
 *  Created on: Sep 16, 2009
 *      Author: monfee
 */

#include "xml2txt.h"
#include "corpus.h"
#include "corpus_txt.h"
#include "sys_file.h"

#include <string.h>
#include <stdexcept>
#include <iostream>

using namespace QLINK;
using namespace std;

xml2txt::xml2txt()
{
	if ((connected_ = client_.connect_server()))
		client_.start_request();
}

xml2txt::~xml2txt()
{
	if (connected_) {
		client_.end_request();
		client_.close_connect();
	}
}

char *xml2txt::convert(char *xml)
{
	client_.send_request(xml);
	client_.recv_text();
    char *text = new (std::nothrow) char [(long)(client_.length() + 1)];
    strncpy(text, client_.text(), client_.length());
    text[client_.length()] = '\0';
	return text;
}

char *xml2txt::gettext(long docid, char *xml)
{
	std::string doc_txt = corpus_txt::instance().id2docpath(docid);
	std::string doc = corpus::instance().id2docpath(docid);
	return gettext(doc.c_str(), doc_txt.c_str(), xml);
}

char *xml2txt::gettearatext(const char *name, char *xml)
{
	return gettext(corpus::instance().name2tearapath(name).c_str(), corpus_txt::instance().name2tearapath(name).c_str(), xml);
}

char *xml2txt::gettext(const char *xmlfile, const char *txtfile, char *xml)
{
	char *content = xml, *text = NULL;

	if (sys_file::exist(txtfile))
		text = sys_file::read_entire_file(txtfile);
	else {
		if (sys_file::exist(xmlfile)) {
			if (connected_) {
				if (!content) {
					content = sys_file::read_entire_file(xmlfile);

					if (!content)
						throw std::runtime_error(std::string("couldn't find file: ") + xmlfile);
				}

				text = convert(content);
				if (!xml)
					delete [] content;
			}
			else
				cerr << "Warning: " << "trying to convert " << xmlfile
						<< " but the XML2TXT server is not ready!" << endl;
		}
		else
			cerr << "Error: " << "no such file - " << xmlfile << endl;
	}
	return text;
}
